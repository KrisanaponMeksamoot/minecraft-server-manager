use std::{collections::{BTreeMap, HashMap}, path::Path, sync::Arc, thread::JoinHandle};
use tokio::{fs, io::AsyncWriteExt, sync::broadcast};
use systemd::{journal::{OpenOptions}};

use crate::systemd1::{Process, UnitStatus};

// The message we'll send over the broadcast channel
#[derive(Clone, Debug, serde::Serialize)]
pub struct LogLine {
    pub message: String,
    pub priority: String,
    pub timestamp: u64,
    pub command: String,
    pub pid: i64
}

impl LogLine {
    pub fn from_journal_entry(entry: &BTreeMap<String, String>, timestamp: u64) -> Self {
        Self {
            message: entry.get("MESSAGE").cloned().unwrap_or_default(),
            priority: entry.get("PRIORITY").cloned().unwrap_or_else(|| "6".to_string()),
            timestamp,
            command:  entry.get("_COMM").cloned().unwrap_or_default(),
            pid: entry.get("_PID").cloned().map_or(-1, |s| s.parse().unwrap_or(-1))
        }
    }
}

#[derive(Debug)]
pub struct JournalBroadcaster {
    pub unit_name: String,
    pub name: String,
    pub tx: broadcast::Sender<LogLine>,
    pub handle: JoinHandle<()>
}

impl JournalBroadcaster {
    pub fn new(unit_name: String, name: String) -> Arc<Self> {
        let (tx, _) = broadcast::channel(1024);
        let c_tx = tx.clone();

        let unit_for_thread = unit_name.clone();
        let name_for_thread = name.clone();
        let handle = std::thread::spawn(move || {
            let run = || -> Result<(), systemd::Error> {
                let mut j = OpenOptions::default()
                        .system(true)
                        .local_only(true)
                        .open()?;
                println!("Journal thread for {} started : {}", &name_for_thread, &unit_for_thread);
                j.match_add("_SYSTEMD_UNIT", unit_for_thread)?;
                j.seek_tail()?;
                let _ = j.previous();

                loop {
                    match j.wait(None)? {
                        systemd::JournalWaitResult::Invalidate => {
                            println!("Journal invalidated for {}, re-seeking...", &name_for_thread);
                            j.seek_tail()?;
                            let _ = j.previous();
                            continue;
                        }
                        _ => {
                            while let Some(entry) = j.next_entry()? {
                                let line = LogLine::from_journal_entry(&entry, j.timestamp_usec()?);
                                // println!("log from {} : {}", &name_for_thread, &line.message);
                                let _ = tx.send(line);
                            }
                        }
                    }
                };
            };
            if let Err(e) = run() {
                eprintln!("Journal thread for {} died: {}", &name_for_thread, e);
            }
            // println!("Journal thread for {} died", &name_for_thread);
        });

        Arc::new(Self { unit_name,  name, tx: c_tx, handle })
    }

    pub fn get_logs(self: &Self, since: u64, until: Option<u64>) -> Result<Vec<LogLine>, std::io::Error> {
        let mut j = OpenOptions::default()
                .system(true)
                .local_only(true)
                .open()?;
        j.match_add("_SYSTEMD_UNIT", self.name.clone())?;
        j.seek_realtime_usec(since)?;
        let mut out = Vec::new();
        while let Some(entry) = j.next_entry()? {
            let line = LogLine::from_journal_entry(&entry, j.timestamp_usec()?);
            if let Some(until) = until && line.timestamp > until { break; }
            out.push(line);
        };
        Ok(out)
    }

    pub fn get_logs_to(
        &self,
        since: u64,
        until: Option<u64>,
        tx: tokio::sync::mpsc::Sender<LogLine>,
    ) -> Result<(), std::io::Error> {
        let mut j = OpenOptions::default().system(true).local_only(true).open()?;
        j.match_add("_SYSTEMD_UNIT", self.unit_name.clone())?;
        j.seek_realtime_usec(since)?;

        while let Some(entry) = j.next_entry()? {
            let timestamp = j.timestamp_usec()?;

            if let Some(u) = until {
                if timestamp > u { break; }
            }

            let line = LogLine::from_journal_entry(&entry, timestamp);

            // blocking_send transfers the data to the async side. 
            // If the receiver is closed (user refreshed), this returns an error.
            if tx.blocking_send(line).is_err() {
                break; 
            }
        }
        Ok(())
    }
}


#[derive(Debug)]
pub struct McsvManager {
    pub dbus: Arc<crate::Systemd1>,

    pub server_names: Vec<String>,
    pub log_broadcasters: HashMap<String, Arc<JournalBroadcaster>>
}


impl McsvManager {
    pub fn new(dbus: Arc<crate::Systemd1>) -> Self {
        Self {
            dbus: dbus,
            server_names: vec![],
            log_broadcasters: HashMap::new()
        }
    }

    pub async fn list_servers(self: &Self) -> Result<Vec<String>, zbus::Error> {
        let patterns= vec!["minecraft@*.service"];
        let units = self.dbus.list_unit_files_by_patterns(&vec![], &patterns).await?;
        
        Ok(units.iter().filter_map(
            |unit| 
                Some(std::path::Path::new(&unit.path)
                .file_name()?
                .to_str()?
                .strip_prefix("minecraft@")?
                .strip_suffix(".service")?
                .to_string())
            ).collect())
    }

    pub async fn get_server_unit_status(self: &Self, name: &str) -> Result<Option<UnitStatus>, zbus::Error> {
        let name = format!("minecraft@{}.service", name);
        let units = self.dbus.list_units_by_names(&vec![&name]).await?;
        Ok(if units.len() < 1 { None } else { Some(units[0].clone()) } )
    }

    pub async fn get_server_process(self: &Self, name: &str) -> Result<Option<Process>, zbus::Error> {
        let name = format!("minecraft@{}.service", name);
        let procs = self.dbus.get_unit_processes(&name).await?;
        Ok(if procs.len() < 1 { None } else { Some(procs[0].clone()) } )
    }

    pub async fn start_server(self: &Self, name: &str) -> Result<(), zbus::Error> {
        let name = format!("minecraft@{}.service", name);
        self.dbus.start_unit(&name, &"replace").await?;
        Ok(())
    }

    pub async fn stop_server(self: &Self, name: &str) -> Result<(), zbus::Error> {
        let name = format!("minecraft@{}.service", name);
        self.dbus.stop_unit(&name, &"replace").await?;
        Ok(())
    }

    pub async fn restart_server(self: &Self, name: &str) -> Result<(), zbus::Error> {
        let name = format!("minecraft@{}.service", name);
        self.dbus.restart_unit(&name, &"replace").await?;
        Ok(())
    }

    pub async fn inject_command(self: &Self, name: &str, cmd: &str) -> Result<(), std::io::Error> {
        let mut file = fs::OpenOptions::new().write(true).create(false)
                .open(Path::new("/run/minecraft/").join(format!("{}.stdin", name))).await?;
        file.write_all(cmd.as_bytes()).await?;
        Ok(())
    }

    pub async fn update(self: &mut Self) -> Result<(), zbus::Error> {
        self.server_names = self.list_servers().await?;
        for name in self.server_names.iter() {
            let unit_name = format!("minecraft@{}.service", name);
            self.log_broadcasters.insert(name.clone(), JournalBroadcaster::new(unit_name, name.clone()));
            // println!("added journal listenter for {}", name);
        }
        Ok(())
    }
}