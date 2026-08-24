(() => {
  const byId = id => document.getElementById(id);
  const card = byId('generator');
  const generate = byId('generate'), copy = byId('copy'), result = byId('result'), message = byId('message');
  let copied = '', clearTimer = 0;

  generate.disabled = false;
  message.textContent = 'Open the full Kry app for generation.';

  byId('reveal').addEventListener('click', () => { const master = byId('master'); master.type = master.type === 'password' ? 'text' : 'password'; byId('reveal').textContent = master.type === 'password' ? 'Reveal' : 'Hide'; });
  generate.addEventListener('click', () => { window.location.href = '/app/'; });
  copy.addEventListener('click', async () => { await navigator.clipboard.writeText(result.textContent); copied = result.textContent; message.textContent = 'Copied; clipboard clears in 20 seconds'; clearTimeout(clearTimer); clearTimer = setTimeout(async () => { try { const current = await navigator.clipboard.readText(); if (current === copied) await navigator.clipboard.writeText(''); } catch (_) {} copied = ''; }, 20000); });
  byId('clear').addEventListener('click', () => { ['site','login','master','exclude'].forEach(id => byId(id).value = ''); result.textContent = 'Your generated password appears here'; message.textContent = 'Cleared'; copy.disabled = true; byId('site').focus(); });
})();
