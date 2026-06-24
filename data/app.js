function togglePassword() {
  var input = document.getElementById('password');
  input.type = input.type === 'password' ? 'text' : 'password';
}

function pollStatus() {
  fetch('/status')
    .then(function(res) { return res.json(); })
    .then(function(data) { updateStatus(data); })
    .catch(function() {
      // Si el microprocesador no responde, asumimos desconectado
      updateStatus({ connected: false, ssid: '', ip: '', reason: 'Sin respuesta del dispositivo' });
    });
}

function updateStatus(data) {
  var dot   = document.getElementById('status-dot');
  var label = document.getElementById('status-label');
  var text  = document.getElementById('status-text');
  var ipEl  = document.getElementById('status-ip');

  // Limpiamos clases previas del círculo de color
  dot.className = 'status-dot';

  if (data.connected) {
    // ESTADO: Conectado (Verde)
    dot.classList.add('dot-ok');
    label.textContent = 'Conectado';
    text.textContent  = 'Red actual: ' + data.ssid;
    ipEl.textContent  = 'Dirección IP: ' + data.ip;
  } else if (data.ssid && data.ssid !== '') {
    // ESTADO: Conectando (Amarillo)
    dot.classList.add('dot-warn');
    label.textContent = 'Conectando';
    text.textContent  = data.reason || 'Intentando enlazar con ' + data.ssid;
    ipEl.textContent  = '';
  } else {
    // ESTADO: Desconectado (Rojo)
    dot.classList.add('dot-error');
    label.textContent = 'Desconectado';
    text.textContent  = 'Ingresá las credenciales para configurar el dispositivo';
    ipEl.textContent  = '';
  }
}

document.addEventListener('DOMContentLoaded', function() {
  pollStatus();
  setInterval(pollStatus, 4000); // Polling rápido cada 4 segundos
});