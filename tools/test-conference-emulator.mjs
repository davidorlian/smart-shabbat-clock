// Run only against the local Firebase Authentication and Realtime Database emulators.
// This creates disposable TEST users and data within the demo project.
import { randomBytes } from 'node:crypto';

const project = 'demo-smart-shabbat-clock';
const database = 'demo-smart-shabbat-clock-default-rtdb';
const authBase = 'http://127.0.0.1:9099';
const dbBase = 'http://127.0.0.1:9000';
let passed = 0;

async function request(url, options = {}) {
  const response = await fetch(url, options);
  const body = await response.text();
  let data;
  try { data = JSON.parse(body); } catch { data = body; }
  return { status: response.status, data };
}

async function account(label, anonymous = false) {
  const payload = anonymous
    ? { returnSecureToken: true }
    : {
      email: label === 'admin' ? 'conference-admin@example.test' : label === 'device' ? 'conference-device@example.test' : `test-${Date.now()}-${Math.random().toString(16).slice(2)}@example.test`,
      password: 'ConferenceTestPass123!',
      returnSecureToken: true
    };
  const response = await request(
    `${authBase}/identitytoolkit.googleapis.com/v1/accounts:signUp?key=demo-api-key`,
    { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) }
  );
  let signedIn = response;
  if (!anonymous && response.status === 400 && response.data?.error?.message === 'EMAIL_EXISTS') {
    signedIn = await request(
      `${authBase}/identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=demo-api-key`,
      { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) }
    );
  }
  if (signedIn.status !== 200 || !signedIn.data.idToken) {
    throw new Error(`${label} auth failed: ${JSON.stringify(signedIn)}`);
  }
  return { uid: signedIn.data.localId, token: signedIn.data.idToken };
}

async function db(method, path, value, token) {
  const url = new URL(`${dbBase}/${path.replace(/^\/+/, '')}.json`);
  url.searchParams.set('ns', database);
  const opts = { method };
  if (token === 'owner') {
    opts.headers = { Authorization: 'Bearer owner' };
  } else if (token) {
    url.searchParams.set('auth', token);
  }
  if (value !== undefined) {
    opts.headers = { ...opts.headers, 'Content-Type': 'application/json' };
    opts.body = JSON.stringify(value);
  }
  return request(url.toString(), opts);
}

function check(label, response, allowed) {
  if (allowed ? response.status !== 200 : response.status !== 401) {
    throw new Error(`${label}: expected ${allowed ? 200 : 401}; got ${response.status}: ${JSON.stringify(response.data)}`);
  }
  console.log(`PASS: ${label}`);
  passed++;
}

function command(uid, seq, mode = 'on') {
  return {
    seq, type: 'relay_mode', payload: { mode },
    createdBy: uid, guest: true,
    createdAt: { '.sv': 'timestamp' }
  };
}

async function main() {
  // An emulator-only safety guard: these ports and IDs cannot target the live project.
  console.log('TESTING LOCAL EMULATORS ONLY:', project);
  const admin = await account('admin');
  const a = await account('visitor A', true);
  const b = await account('visitor B', true);
  const device = await account('device');

  const code = randomBytes(32).toString('hex');
  const secondCode = randomBytes(32).toString('hex');

  check('no anonymous database access',
    await db('GET', 'devices/shabbat-clock-01/state/status'), false);

  // Auth=owner is a special LOCAL emulator superuser credential (not a real secret).
  check('seed test admin role',
    await db('PUT', `roles/admins/${admin.uid}`, true, 'owner'), true);
  check('seed test device metadata',
    await db('PUT', 'devices/shabbat-clock-01/meta', { deviceUid: device.uid, name: 'Test clock' }, 'owner'), true);
  check('seed nextSeq',
    await db('PUT', 'devices/shabbat-clock-01/control/nextSeq', 0, 'owner'), true);

  check('visitor cannot turn on conference',
    await db('PUT', 'conference/enabled', true, a.token), false);
  check('admin opens conference',
    await db('PATCH', 'conference', { enabled: true, joinCode: code, activeVisitor: null }, admin.token), true);
  check('visitor cannot read private code',
    await db('GET', 'conference/joinCode', undefined, a.token), false);
  check('visitor cannot claim control without code',
    await db('PUT', 'conference/activeVisitor', { uid: a.uid }, a.token), false);
  check('visitor cannot claim with wrong code',
    await db('PUT', 'conference/activeVisitor', { uid: a.uid, code: 'a'.repeat(64) }, a.token), false);
  check('visitor cannot claim another user UID',
    await db('PUT', 'conference/activeVisitor', { uid: b.uid, code }, a.token), false);
  check('visitor A claims control using QR code',
    await db('PUT', 'conference/activeVisitor', { uid: a.uid, code }, a.token), true);
  check('visitor A reads active UID',
    await db('GET', 'conference/activeVisitor/uid', undefined, a.token), true);
  check('visitor A cannot read active admission code',
    await db('GET', 'conference/activeVisitor/code', undefined, a.token), false);
  check('visitor A can send command',
    await db('PUT', 'devices/shabbat-clock-01/commands/test-a', command(a.uid, 1), a.token), true);
  check('visitor B takes control using QR',
    await db('PUT', 'conference/activeVisitor', { uid: b.uid, code }, b.token), true);
  check('visitor A cannot send after B takes control',
    await db('PUT', 'devices/shabbat-clock-01/commands/test-a-revoked', command(a.uid, 2), a.token), false);
  check('visitor A cannot reserve sequence',
    await db('PUT', 'devices/shabbat-clock-01/control/nextSeq', 1, a.token), false);
  check('visitor B can send command',
    await db('PUT', 'devices/shabbat-clock-01/commands/test-b', command(b.uid, 2), b.token), true);
  check('device can inspect conference for pending guest commands',
    await db('GET', 'conference', undefined, device.token), true);
  check('admin closes conference',
    await db('PATCH', 'conference', { enabled: false, activeVisitor: null, joinCode: null }, admin.token), true);
  check('guest cannot send after close',
    await db('PUT', 'devices/shabbat-clock-01/commands/test-b-closed', command(b.uid, 3), b.token), false);
  check('guest cannot claim old code after close',
    await db('PUT', 'conference/activeVisitor', { uid: b.uid, code }, b.token), false);
  check('admin starts next conference with fresh QR',
    await db('PATCH', 'conference', { enabled: true, activeVisitor: null, joinCode: secondCode }, admin.token), true);
  check('old printed QR is rejected in a later conference',
    await db('PUT', 'conference/activeVisitor', { uid: a.uid, code }, a.token), false);

  console.log(`\nAll ${passed} checks passed against local Firebase emulators.`);
  console.log('Local browser admin: conference-admin@example.test / ConferenceTestPass123!');
  console.log('Use only http://127.0.0.1:5000 for manual tests.');
}

main().catch((error) => {
  console.error(`\nFAILED: ${error.message}`);
  console.error('Check that the emulator suite was RESTARTED after updating database.rules.json.');
  process.exitCode = 1;
});
