(function () {
  async function bindUpload(formId, resultId) {
    const form = document.getElementById(formId); if (!form) return;
    form.addEventListener('submit', async function (e) {
      e.preventDefault(); const fileInput = form.querySelector('input[type=file]'); const f = fileInput && fileInput.files && fileInput.files[0]; if (!f) { EK.showAlert('Select file', 'error'); return; }
      const b64 = await EK.toBase64(f);
      const data = Object.fromEntries(new FormData(form)); // collects textual fields
      data.file_name = f.name; data.file_base64 = b64; // backend accepts these keys
      const res = await EK.postJson('/upload-material', data);
      const out = document.getElementById(resultId);
      if (res.ok && res.json && res.json.success) { if (out) out.textContent = 'Uploaded'; EK.showAlert('Uploaded', 'success'); form.reset(); loadMaterials(); } else { if (out) out.textContent = 'Upload error'; EK.showAlert((res.json && res.json.message) || 'Upload failed', 'error'); }
    });
  }

  async function loadMaterials() {
    const tid = document.getElementById('teacherId').value;
    if (!tid) return;
    const res = await fetch('http://localhost:8080/get-materials?teacher_id=' + encodeURIComponent(tid));
    const j = await res.json();
    const div = document.getElementById('materialsList');
    if (!div) return;
    div.innerHTML = '';
    if (j.materials && j.materials.length) {
      j.materials.forEach(m => {
        const d = document.createElement('div');
        d.className = 'dashboard-card';
        d.innerHTML = `<div class="space-between"><strong>${m.file_name}</strong><div><a class="btn small" href='/download?id=${m.id}'>Download</a> <button class="btn small" onclick="deleteMaterial(${m.id})">Delete</button></div></div><div class="footer-note">${m.program_name || ''} • ${m.subject_name || ''} • ${EK.formatDate(m.uploaded_at || '')}</div>`;
        div.appendChild(d);
      });
    } else {
      div.innerHTML = '<div class="center">No materials yet</div>';
    }
  }

  async function deleteMaterial(id) {
    if (!confirm('Delete this material?')) return;
    const res = await fetch('http://localhost:8080/delete-material', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id: id }) });
    const j = await res.json();
    if (res.ok) {
      EK.showAlert('Deleted', 'success');
      loadMaterials();
    } else {
      EK.showAlert(j.message || 'Delete failed', 'error');
    }
  }

  window.EK = window.EK || {};
  window.EK.bindUpload = bindUpload;
  window.EK.loadMaterials = loadMaterials;
  window.deleteMaterial = deleteMaterial;
})();
