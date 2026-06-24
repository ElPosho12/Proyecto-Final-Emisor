// ─── 1. Ocultar o Mostrar Contraseña ──────────────────────────────────────────
function togglePassword() {
  var input = document.getElementById('password');
  input.type = input.type === 'password' ? 'text' : 'password';
}

// ─── 2. Consulta Periódica de Estado (Polling) ───────────────────────────────
function pollStatus() {
  fetch('/status')
    .then(function(res) { return res.json(); })
    .then(function(data) { updateStatus(data); })
    .catch(function() {
      // Si el microprocesador no responde (ej. se está reiniciando), asumimos desconectado
      updateStatus({ connected: false, ssid: '', ip: '', reason: 'Sin respuesta del dispositivo' });
    });
}

// ─── 3. Actualización de Interfaz y Control de Animaciones ────────────────────
function updateStatus(data) {
  var dot   = document.getElementById('status-dot');
  var label = document.getElementById('status-label');
  var text  = document.getElementById('status-text');
  var ipEl  = document.getElementById('status-ip');

  // Limpiamos las clases de color previas del círculo testigo
  dot.className = 'status-dot';

  if (data.connected) {
    // ESTADO: Conectado (Círculo Verde)
    dot.classList.add('dot-ok');
    label.textContent = 'Conectado';
    text.textContent  = 'Red actual: ' + data.ssid;
    ipEl.textContent  = 'Dirección IP: ' + data.ip;
    
    // Mantenemos o permitimos que la animación siga activa mostrando el éxito
  } else if (data.ssid && data.ssid !== '') {
    // ESTADO: Conectando (Círculo Amarillo)
    dot.classList.add('dot-warn');
    label.textContent = 'Conectando';
    text.textContent  = data.reason || 'Intentando enlazar con ' + data.ssid;
    ipEl.textContent  = '';
  } else {
    // ESTADO: Desconectado / Error (Círculo Rojo)
    // 🎬 Si la conexión falló o volvió a cero, desarmamos la animación automáticamente
    document.body.classList.remove('loading-active');
    
    dot.classList.add('dot-error');
    label.textContent = 'Desconectado';
    text.textContent  = 'Ingresá las credenciales para configurar el dispositivo';
    ipEl.textContent  = '';
  }
}

// ─── 4. Inicialización de Eventos al Cargar la Página ─────────────────────────
document.addEventListener('DOMContentLoaded', function() {
  // Ejecutar el primer chequeo de estado inmediatamente
  pollStatus();
  
  // Configurar el bucle de polling cada 4 segundos
  setInterval(pollStatus, 4000);

  // Interceptamos el envío del formulario para activar la transición visual
  var form = document.querySelector('form');
  if (form) {
    form.addEventListener('submit', function() {
      // Agrega la clase al body para que el CSS achique el formulario y centre el estado
      document.body.classList.add('loading-active');
    });
  }
});