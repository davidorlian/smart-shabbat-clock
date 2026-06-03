import fs from 'node:fs';
import path from 'node:path';

const DEFAULTS = {
  FIREBASE_PROJECT_ID: 'smart-shabbos-clock',
  FIREBASE_DATABASE_URL: 'https://smart-shabbos-clock-default-rtdb.europe-west1.firebasedatabase.app',
  FIREBASE_ADMIN_UID: 'BMRmeRE63aVWhTXFlb2dPYMwWkC3',
  FIREBASE_DEVICE_UID: 'UvbGDisZe2gh1MAtiX9VEuDDAIN2',
};

function loadEnvLocal() {
  const envPath = path.resolve('.env.local');
  if (!fs.existsSync(envPath)) {
    throw new Error('Missing .env.local. Copy .env.local.example to .env.local and fill the local credentials.');
  }

  const raw = fs.readFileSync(envPath, 'utf8');
  for (const line of raw.split(/\r?\n/)) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith('#')) continue;
    const eq = trimmed.indexOf('=');
    if (eq <= 0) continue;
    const key = trimmed.slice(0, eq).trim();
    let value = trimmed.slice(eq + 1).trim();
    if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith("'") && value.endsWith("'"))) {
      value = value.slice(1, -1);
    }
    if (!(key in process.env)) process.env[key] = value;
  }
}

function env(name, fallback = undefined) {
  const value = process.env[name] ?? fallback;
  if (value === undefined || value === '' || value.startsWith('replace_with_')) {
    throw new Error(`Missing required ${name} in .env.local`);
  }
  return value;
}

function serverTimestamp() {
  return { '.sv': 'timestamp' };
}

async function signIn({ apiKey, email, password, expectedUid, label }) {
  const res = await fetch(`https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=${encodeURIComponent(apiKey)}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ email, password, returnSecureToken: true }),
  });

  const body = await res.json().catch(() => ({}));
  if (!res.ok) {
    throw new Error(`${label} sign-in failed: ${res.status} ${JSON.stringify(body)}`);
  }
  if (body.localId !== expectedUid) {
    throw new Error(`${label} UID mismatch. Expected ${expectedUid}, got ${body.localId}`);
  }
  return body.idToken;
}

function databaseUrl(databaseUrl, pathName, token = undefined) {
  const cleanBase = databaseUrl.replace(/\/$/, '');
  const cleanPath = pathName.replace(/^\/+/, '');
  const url = new URL(`${cleanBase}/${cleanPath}.json`);
  if (token) url.searchParams.set('auth', token);
  return url;
}

async function request({ databaseUrl: baseUrl, token, method, pathName, body }) {
  const options = { method, headers: { 'Content-Type': 'application/json' } };
  if (body !== undefined) options.body = JSON.stringify(body);

  const res = await fetch(databaseUrl(baseUrl, pathName, token), options);
  const text = await res.text();
  let parsed = null;
  if (text) {
    try {
      parsed = JSON.parse(text);
    } catch {
      parsed = text;
    }
  }
  return { ok: res.ok, status: res.status, body: parsed };
}

async function expectAllowed(label, action) {
  const result = await action();
  if (!result.ok) {
    throw new Error(`${label}: expected ALLOW, got HTTP ${result.status} ${JSON.stringify(result.body)}`);
  }
  console.log(`PASS allow: ${label}`);
}

async function expectDenied(label, action) {
  const result = await action();
  if (result.ok) {
    throw new Error(`${label}: expected DENY, got HTTP ${result.status} ${JSON.stringify(result.body)}`);
  }
  console.log(`PASS deny:  ${label}`);
}

function validCommand(seq, createdBy) {
  return {
    seq,
    type: 'relay_mode',
    payload: { mode: 'auto' },
    createdBy,
    createdAt: serverTimestamp(),
  };
}

function statusPayload(lastProcessedSeq) {
  return {
    relay: false,
    relayMode: 2,
    shabbat: false,
    time: '00:00',
    timeValid: true,
    hc12Ok: false,
    lastSeen: serverTimestamp(),
    lastProcessedSeq,
    scheduleRevision: 0,
  };
}

async function main() {
  loadEnvLocal();

  const apiKey = env('FIREBASE_WEB_API_KEY');
  const projectId = env('FIREBASE_PROJECT_ID', DEFAULTS.FIREBASE_PROJECT_ID);
  const databaseUrlValue = env('FIREBASE_DATABASE_URL', DEFAULTS.FIREBASE_DATABASE_URL);
  const adminUid = env('FIREBASE_ADMIN_UID', DEFAULTS.FIREBASE_ADMIN_UID);
  const deviceUid = env('FIREBASE_DEVICE_UID', DEFAULTS.FIREBASE_DEVICE_UID);
  const adminEmail = env('FIREBASE_ADMIN_EMAIL');
  const adminPassword = env('FIREBASE_ADMIN_PASSWORD');
  const deviceEmail = env('FIREBASE_DEVICE_EMAIL');
  const devicePassword = env('FIREBASE_DEVICE_PASSWORD');

  console.log(`Validating RTDB rules against project ${projectId}`);

  const adminToken = await signIn({
    apiKey,
    email: adminEmail,
    password: adminPassword,
    expectedUid: adminUid,
    label: 'admin',
  });
  const deviceToken = await signIn({
    apiKey,
    email: deviceEmail,
    password: devicePassword,
    expectedUid: deviceUid,
    label: 'device',
  });

  const runId = `${Date.now()}`;
  const deviceId = `rules-validation-${runId}`;
  const otherDeviceId = `rules-validation-other-${runId}`;
  const commandId = `cmd-${runId}`;
  const ackId = commandId;

  const req = (token, method, pathName, body) => request({
    databaseUrl: databaseUrlValue,
    token,
    method,
    pathName,
    body,
  });

  await expectAllowed('admin can create validation device metadata', () =>
    req(adminToken, 'PUT', `/devices/${deviceId}/meta`, {
      deviceUid,
      name: 'Rules Validation Device',
    })
  );

  await expectAllowed('admin can create other device metadata for isolation test', () =>
    req(adminToken, 'PUT', `/devices/${otherDeviceId}/meta`, {
      deviceUid: `not-${deviceUid}`,
      name: 'Rules Validation Other Device',
    })
  );

  await expectDenied('unauthenticated user cannot read', () =>
    req(undefined, 'GET', `/devices/${deviceId}/state/status`)
  );

  await expectDenied('unauthenticated user cannot write', () =>
    req(undefined, 'PUT', `/devices/${deviceId}/commands/unauth-${runId}`, validCommand(1, adminUid))
  );

  await expectAllowed('admin can create a valid command', () =>
    req(adminToken, 'PUT', `/devices/${deviceId}/commands/${commandId}`, validCommand(1, adminUid))
  );

  await expectDenied('admin cannot edit an existing command', () =>
    req(adminToken, 'PUT', `/devices/${deviceId}/commands/${commandId}/payload/mode`, 'off')
  );

  await expectDenied('admin cannot write directly to device state', () =>
    req(adminToken, 'PUT', `/devices/${deviceId}/state/status`, statusPayload(0))
  );

  await expectAllowed('device user can read its own commands', () =>
    req(deviceToken, 'GET', `/devices/${deviceId}/commands`)
  );

  await expectDenied('device user cannot read another device commands', () =>
    req(deviceToken, 'GET', `/devices/${otherDeviceId}/commands`)
  );

  await expectAllowed('device user can write its own state', () =>
    req(deviceToken, 'PUT', `/devices/${deviceId}/state/status`, statusPayload(1))
  );

  await expectDenied('device user cannot write another device state', () =>
    req(deviceToken, 'PUT', `/devices/${otherDeviceId}/state/status`, statusPayload(1))
  );

  await expectAllowed('admin can read device state', () =>
    req(adminToken, 'GET', `/devices/${deviceId}/state/status`)
  );

  await expectAllowed('device user can write an ACK for an existing command', () =>
    req(deviceToken, 'PUT', `/devices/${deviceId}/acks/${ackId}`, {
      seq: 1,
      ok: true,
      code: 'applied',
      message: 'rules validation ack',
      ackedAt: serverTimestamp(),
    })
  );

  await expectDenied('device user cannot create commands', () =>
    req(deviceToken, 'PUT', `/devices/${deviceId}/commands/device-created-${runId}`, {
      seq: 2,
      type: 'relay_mode',
      payload: { mode: 'off' },
      createdBy: deviceUid,
      createdAt: serverTimestamp(),
    })
  );

  await expectDenied('invalid command payload is rejected', () =>
    req(adminToken, 'PUT', `/devices/${deviceId}/commands/invalid-${runId}`, {
      seq: 3,
      type: 'relay_mode',
      payload: { mode: 'invalid' },
      createdBy: adminUid,
      createdAt: serverTimestamp(),
    })
  );

  await expectDenied('invalid schedule event key is rejected', () =>
    req(adminToken, 'PUT', `/devices/${deviceId}/commands/invalid-schedule-${runId}`, {
      seq: 4,
      type: 'replace_schedule',
      payload: {
        baseScheduleRevision: 0,
        events: {
          32: { day: 0, hour: 12, minute: 0, state: 'on' },
        },
      },
      createdBy: adminUid,
      createdAt: serverTimestamp(),
    })
  );

  console.log('');
  console.log('All real-project RTDB rules validation checks passed.');
  console.log(`Validation data was written under /devices/${deviceId}.`);
  console.log('Command and ACK test records are intentionally not cleaned up because rules make commands/ACKs create-only.');
}

main().catch((err) => {
  console.error('');
  console.error('VALIDATION FAILED');
  console.error(err.message);
  process.exit(1);
});
