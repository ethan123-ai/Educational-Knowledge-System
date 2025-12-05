(function () {
  async function loadMaterials(targetId, filters) {
    let qs = '';
    if (filters) { qs = Object.keys(filters).map(k => k + '=' + encodeURIComponent(filters[k])).join('&'); if (qs) qs = '?' + qs; }
    const res = await fetch('http://localhost:8080/get-materials' + qs);
  }

  async function loadCategories() {
    try {
      const res = await fetch('http://localhost:8080/get-materials');
      const j = await res.json();
      const select = document.getElementById('categorySelect');
      if (!select) return;
      const categories = new Set();
      if (j.materials) {
        j.materials.forEach(m => { if (m.category) categories.add(m.category); });
      }
      categories.forEach(cat => {
        const opt = document.createElement('option');
        opt.value = cat;
        opt.textContent = cat;
        select.appendChild(opt);
      });
    } catch (e) { EK.showAlert('Error loading categories', 'error'); }
  }

  window.EK = window.EK || {};
  window.EK.loadMaterials = loadMaterials;
  window.EK.loadCategories = loadCategories;
})();
