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

    // Ya conectado de verdad: podemos soltar la animación de carga
    document.body.classList.remove('loading-active');

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

// ─── 4. Mostrar un error de envío sin recargar la página ──────────────────────
function mostrarErrorEnvio(mensaje) {
  var dot   = document.getElementById('status-dot');
  var label = document.getElementById('status-label');
  var text  = document.getElementById('status-text');
  var ipEl  = document.getElementById('status-ip');

  document.body.classList.remove('loading-active');
  dot.className = 'status-dot dot-error';
  label.textContent = 'Error';
  text.textContent  = mensaje;
  ipEl.textContent  = '';
}

// ─── 5. Inicialización de Eventos al Cargar la Página ─────────────────────────
document.addEventListener('DOMContentLoaded', function() {
  // Ejecutar el primer chequeo de estado inmediatamente
  pollStatus();

  // Configurar el bucle de polling cada 4 segundos
  setInterval(pollStatus, 4000);

  // Interceptamos el envío del formulario para que NO navegue a /save
  // (antes, al navegar, el navegador mostraba el JSON crudo de la respuesta)
  var form = document.querySelector('form');
  if (form) {
    form.addEventListener('submit', function(e) {
      e.preventDefault(); // clave: evita la navegación cruda a /save

      // Activamos la animación visual (achica el form, agranda el estado)
      document.body.classList.add('loading-active');

      var dot   = document.getElementById('status-dot');
      var label = document.getElementById('status-label');
      var text  = document.getElementById('status-text');
      dot.className = 'status-dot dot-warn';
      label.textContent = 'Guardando';
      text.textContent  = 'Enviando credenciales al dispositivo...';

      var datos = new URLSearchParams(new FormData(form));

      fetch('/save', {
        method: 'POST',
        body: datos
      })
      .then(function(res) {
        if (!res.ok) {
          return res.text().then(function(msg) {
            throw new Error(msg || 'No se pudieron guardar las credenciales.');
          });
        }
        return res.json();
      })
      .then(function() {
        label.textContent = 'Conectando';
        text.textContent  = 'Credenciales guardadas. Conectando a la red...';
        // A partir de acá, el polling normal (cada 4s) va reflejando el estado real
        pollStatus();
      })
      .catch(function(err) {
        mostrarErrorEnvio(err.message);
      });
    });
  }
});