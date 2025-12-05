// E-KNOWS main helpers
(function (window) {
  // API Helpers
  async function postJson(url, data, btn = null) {
    if (btn) setButtonLoading(btn, true);
    try {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
      });
      const txt = await res.text();
      try {
        return { ok: res.ok, status: res.status, json: JSON.parse(txt) };
      } catch (e) {
        return { ok: res.ok, status: res.status, text: txt };
      }
    } finally {
      if (btn) setButtonLoading(btn, false);
    }
  }

  async function getJson(url, btn = null) {
    if (btn) setButtonLoading(btn, true);
    try {
      const res = await fetch('http://localhost:8080' + url);
      return await res.json();
    } finally {
      if (btn) setButtonLoading(btn, false);
    }
  }

  // UI Helpers
  function showAlert(msg, type) {
    const el = document.createElement('div');
    el.className = 'alert ' + (type || '');
    el.textContent = msg;
    const c = document.querySelector('.container') || document.body;
    c.prepend(el);
    el.style.opacity = '0';
    requestAnimationFrame(() => {
      el.style.opacity = '1';
      el.style.transition = 'opacity 0.3s ease';
    });
    setTimeout(() => {
      el.style.opacity = '0';
      setTimeout(() => el.remove(), 300);
    }, 4700);
  }

  function setButtonLoading(btn, loading) {
    if (!btn) return;
    btn.disabled = loading;
    if (loading) {
      btn.classList.add('loading');
      btn._originalText = btn.textContent;
      btn.textContent = '';
    } else {
      btn.classList.remove('loading');
      if (btn._originalText) btn.textContent = btn._originalText;
    }
  }

  // File Helpers
  function toBase64(file) {
    return new Promise((res, rej) => {
      const r = new FileReader();
      r.onload = () => res(r.result.split(',')[1]);
      r.onerror = e => rej(e);
      r.readAsDataURL(file);
    });
  }

  function sanitizeFilename(name) {
    return name.replace(/[^a-zA-Z0-9._-]/g, '_').substring(0, 200);
  }

  // User Interaction
  function confirmAction(message, callback) {
    const confirmed = confirm(message);
    if (confirmed && callback) callback();
    return confirmed;
  }

  // Date Formatting
  function formatDate(iso, format = 'long') {
    try {
      const d = new Date(iso);
      if (format === 'short') return d.toLocaleDateString();
      if (format === 'time') return d.toLocaleTimeString();
      return d.toLocaleString();
    } catch (e) {
      return iso;
    }
  }

  // Form Validation
  function validateForm(form) {
    const inputs = form.querySelectorAll('input[required], select[required], textarea[required]');
    let valid = true;
    inputs.forEach(input => {
      if (!input.value.trim()) {
        valid = false;
        showInputError(input, 'This field is required');
      } else {
        clearInputError(input);
      }
    });
    return valid;
  }

  function showInputError(input, message) {
    clearInputError(input);
    input.classList.add('error');
    const error = document.createElement('div');
    error.className = 'input-error';
    error.textContent = message;
    input.parentNode.appendChild(error);
  }

  function clearInputError(input) {
    input.classList.remove('error');
    const error = input.parentNode.querySelector('.input-error');
    if (error) error.remove();
  }

  // Hamburger Menu Toggle
  function initHamburgerMenu() {
    const hamburger = document.getElementById('hamburger');
    const sidebar = document.getElementById('sidebar');

    if (hamburger && sidebar) {
      hamburger.addEventListener('click', () => {
        hamburger.classList.toggle('active');
        sidebar.classList.toggle('active');
      });

      // Close sidebar when clicking outside
      document.addEventListener('click', (e) => {
        if (!hamburger.contains(e.target) && !sidebar.contains(e.target)) {
          hamburger.classList.remove('active');
          sidebar.classList.remove('active');
        }
      });
    }
  }

  // Initialize on DOM ready
  document.addEventListener('DOMContentLoaded', initHamburgerMenu);

  // expose
  window.EK = window.EK || {};
  window.EK.postJson = postJson;
  window.EK.getJson = getJson;
  window.EK.showAlert = showAlert;
  window.EK.toBase64 = toBase64;
  window.EK.confirmAction = confirmAction;
  window.EK.sanitizeFilename = sanitizeFilename;
  window.EK.formatDate = formatDate;
  window.EK.validateForm = validateForm;
  window.EK.showInputError = showInputError;
  window.EK.clearInputError = clearInputError;
  window.EK.setButtonLoading = setButtonLoading;
})(window);
