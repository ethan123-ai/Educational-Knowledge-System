(function () {
  async function loadSubjects(teacherId, containerId) {
    try {
      const res = await fetch('http://localhost:8080/api/teacher/get-subjects?teacher_id=' + encodeURIComponent(teacherId));
      const j = await res.json(); const container = document.getElementById(containerId);
      if (!container) return;
      container.innerHTML = '';
      if (j.subjects && j.subjects.length) {
        const table = document.createElement('table'); table.className = 'table'; const thead = document.createElement('thead'); thead.innerHTML = '<tr><th>Subject</th><th>Grade</th><th>Semester</th></tr>'; table.appendChild(thead); const tb = document.createElement('tbody');
        j.subjects.forEach(s => { const tr = document.createElement('tr'); tr.innerHTML = `<td>${s.subject}</td><td>${s.grade_level || ''}</td><td>${s.semester || ''}</td>`; tb.appendChild(tr); });
      } else container.innerHTML = '<div class="center">No subjects assigned</div>';
    } catch (e) { EK.showAlert('Error loading subjects', 'error'); }
  }
  async function addSubject(data) { const res = await EK.postJson('/api/teacher/add-subject', data); if (res.ok && res.json && res.json.success) EK.showAlert('Subject added', 'success'); else EK.showAlert('Error adding subject', 'error'); }

  async function loadAllSubjects() {
    const res = await fetch('http://localhost:8080/api/admin/get-subjects');
    const j = await res.json();
    const select = document.getElementById('assign-subject-select');
    if (!select) return;
    select.innerHTML = '<option value="">Select Subject to Assign</option>';
    if (j.subjects && j.subjects.length) {
      j.subjects.forEach(s => {
        const opt = document.createElement('option');
        opt.value = s.id;
        opt.textContent = `${s.subject} (${s.program}, ${s.grade_level}, ${s.semester})`;
        select.appendChild(opt);
      });
    }
  }

  async function assignSubject() {
    const select = document.getElementById('assign-subject-select');
    const subjectId = select.value;
    if (!subjectId) { alert('Please select a subject'); return; }
    const tid = document.getElementById('teacherId').value;
    const res = await fetch('http://localhost:8080/api/teacher/assign-subject', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ subject_id: subjectId, teacher_id: tid }) });
    const j = await res.json();
    if (res.ok) {
      alert('Subject assigned');
      loadSubjectsTable();
      select.value = '';
    } else {
      alert(j.message || 'Assign failed');
    }
  }

  async function deleteSubject(id) {
    const tid = document.getElementById('teacherId').value;
    const res = await fetch('http://localhost:8080/api/teacher/delete-subject', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ id: id, teacher_id: tid }) });
    const j = await res.json();
    if (res.ok) {
      alert('Subject deleted');
      loadSubjectsTable();
    } else {
      alert(j.message || 'Delete failed');
    }
  }

  window.EK = window.EK || {};
  window.EK.loadSubjects = loadSubjects; window.EK.addSubject = addSubject; window.EK.loadAllSubjects = loadAllSubjects; window.assignSubject = assignSubject; window.deleteSubject = deleteSubject;
})();
