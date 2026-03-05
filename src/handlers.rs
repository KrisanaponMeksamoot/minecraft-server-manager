use std::{error::Error, fmt, ops::{IndexMut}, sync::Arc};

use futures_util::{SinkExt, StreamExt};
use sysinfo::{CpuRefreshKind, MemoryRefreshKind, RefreshKind, System};

use axum::{
    Json, extract::{Path, State, ws::{Message, WebSocket, WebSocketUpgrade}},
    http::StatusCode,
    response::{IntoResponse, Response}
};
use thiserror::Error;
use serde_json::{Value, json};
use tokio::sync::Mutex;

use crate::{AppState, mcsv_mgr::{JournalBroadcaster}};

#[derive(Debug)] // Required for the Error trait
pub enum ApiError {
    NotFound,
    Invalid
}

impl fmt::Display for ApiError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            ApiError::NotFound => write!(f, "Not found"),
            ApiError::Invalid => write!(f, "Invalid")
        }
    }
}

// Implement the Error trait
impl Error for ApiError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        None
    }
}

#[derive(Error, Debug)]
pub enum AppError {
    #[error("Systemd D-Bus error: {0}")]
    Dbus(#[from] zbus::Error),
    
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("API error: {0}")]
    Api(#[from] ApiError),

    #[error("Unknown error")]
    Unknown
}

impl IntoResponse for AppError {
    fn into_response(self) -> Response {
        if let AppError::Api(aer) = self {
            return (
                match aer {
                    ApiError::NotFound => StatusCode::NOT_FOUND,
                    ApiError::Invalid => StatusCode::NOT_ACCEPTABLE
                },
                "API Error"
            ).into_response();
        }

        eprintln!("Internal Error: {}", self);

        (
            StatusCode::INTERNAL_SERVER_ERROR,
            format!("Something went wrong: {}", self),
        )
            .into_response()
    }
}

pub async fn get_status() -> Json<Value> {
    let mut sys = System::new_with_specifics(RefreshKind::nothing()
        .with_memory(MemoryRefreshKind::everything())
        .with_cpu(CpuRefreshKind::everything())
    );
    sys.refresh_memory();
    sys.refresh_cpu_all();
    let res = Json(json!({
        "MemTotal": sys.total_memory() / 1024,
        "MemFree": sys.free_memory() / 1024,
        "MemAvailable": sys.available_memory() / 1024,
        "SwapTotal": sys.total_swap() / 1024,
        "SwapFree": sys.free_swap() / 1024,
        "GlobalCpuUsage": sys.global_cpu_usage()
    }));
    // let cpu_usage: Vec<Value> = sys.cpus().iter().map(|cpu| json!(cpu.cpu_usage())).collect();
    // *res.index_mut("CpusUsage") = Value::Array(cpu_usage);
    res
}

pub async fn list_servers(state: State<Arc<AppState>>) -> Result<Json<Value>, AppError> {
    let servers = state.mcsv_mgr.lock().await
        .server_names.iter().map(|name| json!(name)).collect();
    
    Ok(Json(Value::Array(servers)))
}

pub async fn get_server_status(id: Path<String>, state: State<Arc<AppState>>) -> Result<Json<Value>, AppError> {
    let unit = state.mcsv_mgr.lock().await.get_server_unit_status(&id.0).await?;

    let unit = if let Some(unit) = unit { unit } else { return Err(AppError::Api(ApiError::NotFound)); };

    let mut res = Json(json!({}));
    
    *res.index_mut("unit") = json!({
        "name": unit.name,
        "load_state": unit.load_state,
        "active_state": unit.active_state,
        "sub_state": unit.sub_state
    });

    Ok(res)
}

pub async fn handle_server_action(
    Path((id, action)): Path<(String, String)>,
    State(state): State<Arc<AppState>>,
) -> Result<(), AppError> {
    println!("server action {} to {}", action, id);
    let _job_path = match action.as_str() {
        "start" => state.mcsv_mgr.lock().await.start_server(&id).await?,
        "stop" => state.mcsv_mgr.lock().await.stop_server(&id).await?,
        "restart" => state.mcsv_mgr.lock().await.restart_server(&id).await?,
        _ => return Err(AppError::Api(ApiError::NotFound)),
    };

    Ok(())
}

pub async fn handle_server_console(Path(id): Path<String>, state: State<Arc<AppState>>, ws: WebSocketUpgrade) -> Result<impl axum::response::IntoResponse, AppError> {
    let state = state.0;
    // println!("connecting to {}", id);
    let jb = {
        let mcsv_mgr = state.mcsv_mgr.lock().await;
        let lb = mcsv_mgr.log_broadcasters.get(&id);
        if let Some(lb) = lb { lb.clone() } else { return Err(AppError::Api(ApiError::NotFound)); }
    };
    Ok(ws.on_upgrade(|s| handle_server_console_socket(s, state, jb)))
}

pub async fn handle_server_console_socket(socket: WebSocket, state: Arc<AppState>, jb: Arc<JournalBroadcaster>) {
    let (sender, mut receiver) = socket.split();
    let mut rx = jb.tx.subscribe();
    let sender = Arc::new(Mutex::new(sender));

    let sender0 = sender.clone();
    let mut send_task = tokio::spawn(async move {
        while let Ok(logline) = rx.recv().await {
            let msg = json!({
                "type": "log",
                "message": logline.message,
                "comm": logline.command,
                "pid": logline.pid
            });
            let msg = serde_json::to_string(&msg);
            let msg = if let Ok(msg) = msg { msg } else { break; };
            let res = sender0.lock().await.send(Message::Text(msg.into())).await;
            if res.is_err() {
                break;
            }
        };
    });

    let name = jb.name.clone();
    let mut recv_task = tokio::spawn(async move {
        while let Some(Ok(Message::Text(text))) = receiver.next().await {
            println!("Received command for {}: {}", name, text);
            let cmd = if !text.ends_with('\n') { text.to_string() + "\n" } else { text.to_string() };
            
            let res = state.mcsv_mgr.lock().await.inject_command(&name, &cmd).await;
            if let Err(e) = res {
                let msg = format!("Failed to inject command: {}", e);
                eprintln!("{}", &msg);
                let msg = json!({
                    "type": "error",
                    "message": msg
                });
                let msg = serde_json::to_string(&msg);
                let msg = if let Ok(msg) = msg { msg } else { break; };
                let res = sender.lock().await.send(Message::Text(msg.into())).await;
                if res.is_err() {
                    break;
                }
            }
        }
    });
    // println!("connected to {}", &jb.name);

    // If either task finishes (disconnect or error), abort the other
    tokio::select! {
        _ = (&mut send_task) => recv_task.abort(),
        _ = (&mut recv_task) => send_task.abort(),
    };
}