(function () {
  // client-side attempt tracking (complements server-side)
  function bindStudentValidation(formId, maxAttempts = 3) {
    const form = document.getElementById(formId);
    if (!form) return;

    const key = 'ek_student_attempts';
    const state = JSON.parse(localStorage.getItem(key) || '{}');

    form.addEventListener('submit', async function (e) {
      e.preventDefault();

      if (!EK.validateForm(form)) {
        return;
      }

      const data = Object.fromEntries(new FormData(form));
      const payload = {
        program: data.program || '',
        grade_level: data.grade_level || '',
        semester: data.semester || '',
        teacher_id: data.teacher_id || '',
        access_code: data.access_code || ''
      };

      const attemptKey = `${payload.teacher_id}|${payload.program}|${payload.grade_level}`;

      if (state[attemptKey] && state[attemptKey] >= maxAttempts) {
        EK.showAlert('Access blocked after too many attempts. Please contact your teacher.', 'error');
        return;
      }

      const submitBtn = form.querySelector('button[type="submit"]');
      const res = await EK.postJson('/api/student/validate', payload, submitBtn);

      if (res.ok && res.json && res.json.success) {
        EK.showAlert('Access granted! Redirecting...', 'success');
        state[attemptKey] = 0;
        localStorage.setItem(key, JSON.stringify(state));
        setTimeout(() => {
          window.location.href = '/templates/student_materials.html';
        }, 1000);
      } else {
        state[attemptKey] = (state[attemptKey] || 0) + 1;
        localStorage.setItem(key, JSON.stringify(state));

        const remainingAttempts = maxAttempts - state[attemptKey];
        if (remainingAttempts > 0) {
          EK.showAlert(`Invalid access code. ${remainingAttempts} attempt${remainingAttempts === 1 ? '' : 's'} remaining.`, 'error');
        } else {
          EK.showAlert('You have been blocked after too many failed attempts. Please contact your teacher.', 'error');
        }
      }
    });
  }
  window.EK = window.EK || {};
  window.EK.bindStudentValidation = bindStudentValidation;
})();