// Mostrar / ocultar contraseña
function togglePassword() {
  var input = document.getElementById('password');
  input.type = input.type === 'password' ? 'text' : 'password';
}

// Polling de estado cada 5 segundos
function pollStatus() {
  fetch('/status')
    .then(function(res) { return res.json(); })
    .then(function(data) { updateStatus(data); })
    .catch(function() {
      updateStatus({ connected: false, ssid: '', ip: '', reason: 'Sin respuesta del dispositivo' });
    });
}

function updateStatus(data) {
  var dot   = document.getElementById('status-dot');
  var label = document.getElementById('status-label');
  var text  = document.getElementById('status-text');
  var ipEl  = document.getElementById('status-ip');

  dot.className = 'status-dot';

  if (data.connected) {
    dot.classList.add('dot-ok');
    label.textContent = 'Conectado';
    text.textContent  = 'Red: ' + data.ssid;
    ipEl.textContent  = 'IP: ' + data.ip;
  } else if (data.ssid && data.ssid !== '') {
    dot.classList.add('dot-warn');
    label.textContent = 'Conectando…';
    text.textContent  = data.reason || 'Intentando conectar a ' + data.ssid;
    ipEl.textContent  = '';
  } else {
    dot.classList.add('dot-error');
    label.textContent = 'Sin configurar';
    text.textContent  = 'Ingresá el nombre y contraseña del WiFi';
    ipEl.textContent  = '';
  }
}

// Arrancar polling al cargar la página
document.addEventListener('DOMContentLoaded', function() {
  pollStatus();
  setInterval(pollStatus, 5000);
});
