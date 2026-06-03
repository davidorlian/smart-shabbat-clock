import { initializeApp } from 'https://www.gstatic.com/firebasejs/12.7.0/firebase-app.js';
import {
  getAuth,
  onAuthStateChanged,
  signInWithEmailAndPassword,
  signOut
} from 'https://www.gstatic.com/firebasejs/12.7.0/firebase-auth.js';
import {
  getDatabase,
  ref,
  onValue,
  runTransaction,
  set,
  serverTimestamp
} from 'https://www.gstatic.com/firebasejs/12.7.0/firebase-database.js';
import { firebaseConfig } from './firebase-config.js';

const DEVICE_ID = 'shabbat-clock-01';

const app = initializeApp(firebaseConfig);
const auth = getAuth(app);
const db = getDatabase(app);

const authPanel = document.getElementById('authPanel');
const controlPanel = document.getElementById('controlPanel');
const signInForm = document.getElementById('signInForm');
const signOutButton = document.getElementById('signOutButton');
const connectionLabel = document.getElementById('connectionLabel');
const message = document.getElementById('message');
const scheduleList = document.getElementById('scheduleList');
const replaceScheduleButton = document.getElementById('replaceScheduleButton');

let currentUser = null;
let currentSchedule = { revision: 0, events: [] };
let lastCommandUnsubscribe = null;
let statusUnsubscribe = null;
let scheduleUnsubscribe = null;

const fields = {
  relay: document.getElementById('relayStatus'),
  relayMode: document.getElementById('relayModeStatus'),
  shabbat: document.getElementById('shabbatStatus'),
  time: document.getElementById('timeStatus'),
  timeValid: document.getElementById('timeValidStatus'),
  hc12: document.getElementById('hc12Status'),
  lastSeq: document.getElementById('lastSeqStatus'),
  scheduleRevision: document.getElementById('scheduleRevisionStatus'),
  lastSeen: document.getElementById('lastSeen'),
  scheduleCount: document.getElementById('scheduleCount'),
  lastCommandId: document.getElementById('lastCommandId'),
  lastCommandSeq: document.getElementById('lastCommandSeq'),
  lastCommandType: document.getElementById('lastCommandType'),
  ackStatus: document.getElementById('ackStatus')
};

function setMessage(text, isError = false) {
  message.textContent = text;
  message.classList.toggle('error', isError);
}

function formatBool(value) {
  if (value === true) return 'Yes';
  if (value === false) return 'No';
  return '--';
}

function formatRelayMode(mode) {
  if (mode === 0) return 'Off';
  if (mode === 1) return 'On';
  if (mode === 2) return 'Auto';
  return '--';
}

function formatTimestamp(value) {
  if (!value) return 'Waiting for device';
  return new Date(value).toLocaleString();
}

function normalizeEvents(events) {
  if (!events) return [];
  if (Array.isArray(events)) return events.filter(Boolean);
  return Object.keys(events)
    .sort((a, b) => Number(a) - Number(b))
    .map((key) => events[key])
    .filter(Boolean);
}

function renderStatus(status) {
  const data = status || {};
  fields.relay.textContent = data.relay ? 'On' : 'Off';
  fields.relayMode.textContent = formatRelayMode(data.relayMode);
  fields.shabbat.textContent = formatBool(data.shabbat);
  fields.time.textContent = data.time || '--';
  fields.timeValid.textContent = formatBool(data.timeValid);
  fields.hc12.textContent = formatBool(data.hc12Ok);
  fields.lastSeq.textContent = Number.isFinite(data.lastProcessedSeq) ? data.lastProcessedSeq : '--';
  fields.scheduleRevision.textContent = Number.isFinite(data.scheduleRevision) ? data.scheduleRevision : '--';
  fields.lastSeen.textContent = formatTimestamp(data.lastSeen);
}

function renderSchedule(schedule) {
  currentSchedule = {
    revision: Number.isFinite(schedule?.revision) ? schedule.revision : 0,
    events: normalizeEvents(schedule?.events)
  };

  fields.scheduleCount.textContent = `${currentSchedule.events.length} event${currentSchedule.events.length === 1 ? '' : 's'}`;
  scheduleList.innerHTML = '';

  if (currentSchedule.events.length === 0) {
    const empty = document.createElement('p');
    empty.textContent = 'No schedule published by the device yet.';
    empty.className = 'message';
    scheduleList.appendChild(empty);
    return;
  }

  const dayNames = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
  currentSchedule.events.forEach((event) => {
    const row = document.createElement('div');
    row.className = 'schedule-item';
    const time = `${String(event.hour).padStart(2, '0')}:${String(event.minute).padStart(2, '0')}`;
    row.innerHTML = `<strong>${dayNames[event.day] || '--'} ${time}</strong><span>${event.state || '--'}</span>`;
    scheduleList.appendChild(row);
  });
}

function buildPayload(type, mode) {
  if (type === 'relay_mode') return { mode };
  if (type === 'shabbat_mode') return { mode };
  if (type === 'replace_schedule') {
    if (currentSchedule.events.length === 0) {
      throw new Error('No schedule snapshot is available to send yet');
    }
    return {
      baseScheduleRevision: currentSchedule.revision,
      events: currentSchedule.events.slice(0, 32).map((event) => ({
        day: Number(event.day),
        hour: Number(event.hour),
        minute: Number(event.minute),
        state: event.state === 'on' ? 'on' : 'off'
      }))
    };
  }
  throw new Error(`Unsupported command type: ${type}`);
}

async function nextCommandSeq() {
  const seqRef = ref(db, `/devices/${DEVICE_ID}/control/nextSeq`);
  const result = await runTransaction(seqRef, (current) => {
    if (current === null) return 0;
    return Number(current || 0) + 1;
  });

  if (!result.committed) throw new Error('Sequence transaction was not committed');
  return result.snapshot.val();
}

function watchAck(commandId) {
  if (lastCommandUnsubscribe) lastCommandUnsubscribe();
  fields.ackStatus.textContent = 'Waiting';

  lastCommandUnsubscribe = onValue(ref(db, `/devices/${DEVICE_ID}/acks/${commandId}`), (snapshot) => {
    if (!snapshot.exists()) return;
    const ack = snapshot.val();
    fields.ackStatus.textContent = `${ack.ok ? 'OK' : 'Failed'}: ${ack.code || ''} ${ack.message || ''}`.trim();
  });
}

async function sendCommand(type, mode = undefined) {
  if (!currentUser) throw new Error('Sign in first');

  const seq = await nextCommandSeq();
  const commandId = `c${String(seq).padStart(9, '0')}`;
  const command = {
    seq,
    type,
    payload: buildPayload(type, mode),
    createdBy: currentUser.uid,
    createdAt: serverTimestamp()
  };

  await set(ref(db, `/devices/${DEVICE_ID}/commands/${commandId}`), command);

  fields.lastCommandId.textContent = commandId;
  fields.lastCommandSeq.textContent = seq;
  fields.lastCommandType.textContent = type;
  watchAck(commandId);
  setMessage(`Command ${commandId} created.`);
}

signInForm.addEventListener('submit', async (event) => {
  event.preventDefault();
  setMessage('');
  const email = document.getElementById('emailInput').value;
  const password = document.getElementById('passwordInput').value;

  try {
    await signInWithEmailAndPassword(auth, email, password);
  } catch (error) {
    setMessage(error.message, true);
  }
});

signOutButton.addEventListener('click', () => {
  signOut(auth).catch((error) => setMessage(error.message, true));
});

document.querySelectorAll('[data-command]').forEach((button) => {
  button.addEventListener('click', async () => {
    setMessage('');
    button.disabled = true;
    try {
      await sendCommand(button.dataset.command, button.dataset.mode);
    } catch (error) {
      setMessage(error.message, true);
    } finally {
      button.disabled = false;
    }
  });
});

replaceScheduleButton.addEventListener('click', async () => {
  setMessage('');
  replaceScheduleButton.disabled = true;
  try {
    await sendCommand('replace_schedule');
  } catch (error) {
    setMessage(error.message, true);
  } finally {
    replaceScheduleButton.disabled = false;
  }
});

onAuthStateChanged(auth, (user) => {
  currentUser = user;
  if (statusUnsubscribe) statusUnsubscribe();
  if (scheduleUnsubscribe) scheduleUnsubscribe();
  if (lastCommandUnsubscribe) lastCommandUnsubscribe();
  statusUnsubscribe = null;
  scheduleUnsubscribe = null;
  lastCommandUnsubscribe = null;

  authPanel.hidden = !!user;
  controlPanel.hidden = !user;
  signOutButton.hidden = !user;
  connectionLabel.textContent = user ? `Signed in as ${user.email}` : 'Not connected';

  if (!user) {
    setMessage('');
    return;
  }

  statusUnsubscribe = onValue(ref(db, `/devices/${DEVICE_ID}/state/status`), (snapshot) => {
    renderStatus(snapshot.val());
  }, (error) => setMessage(error.message, true));

  scheduleUnsubscribe = onValue(ref(db, `/devices/${DEVICE_ID}/state/schedule`), (snapshot) => {
    renderSchedule(snapshot.val());
  }, (error) => setMessage(error.message, true));
});
