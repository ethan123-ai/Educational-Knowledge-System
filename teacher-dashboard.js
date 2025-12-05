x (function () {
  async function loadTeacherDashboard(teacherId) {
    if (!teacherId) return;
    try {
      const res = await EK.postJson('/api/teacher/dashboard-data', { teacher_id: teacherId });
      if (res.json && res.json.total !== undefined) { const el = document.getElementById('teacherInfo'); if (el) el.textContent = 'Total uploads: ' + res.json.total; }
      // materials
      const mres = await fetch('http://localhost:8080/get-materials?teacher_id=' + encodeURIComponent(teacherId));
      // chart
      if (res.json && res.json.stats) {
        const ctx = document.getElementById('uploadChart');
        if (ctx) {
          const chart = new Chart(ctx, {
            type: 'bar',
            data: {
              labels: Object.keys(res.json.stats),
              datasets: [{
                label: 'Uploads',
                data: Object.values(res.json.stats),
                backgroundColor: '#f7e14d',
                borderColor: '#d31907',
                borderWidth: 1
              }]
            },
            options: {
              responsive: true,
              scales: {
                y: { beginAtZero: true }
              }
            }
          });
        }
      }
    } catch (e) { EK.showAlert('Error loading dashboard', 'error'); }
  }
  window.EK = window.EK || {};
  window.EK.loadTeacherDashboard = loadTeacherDashboard;
})();
