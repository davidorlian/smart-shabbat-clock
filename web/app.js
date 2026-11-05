import { initializeApp } from 'https://www.gstatic.com/firebasejs/12.7.0/firebase-app.js';
import {
  getAuth,
  onAuthStateChanged,
  signInWithEmailAndPassword,
  signInAnonymously,
  signOut,
  connectAuthEmulator
} from 'https://www.gstatic.com/firebasejs/12.7.0/firebase-auth.js';
import {
  getDatabase,
  ref,
  get,
  onValue,
  runTransaction,
  set,
  update,
  serverTimestamp,
  connectDatabaseEmulator
} from 'https://www.gstatic.com/firebasejs/12.7.0/firebase-database.js';
import { firebaseConfig } from './firebase-config.js';

const DEVICE_ID = 'shabbat-clock-01';
const MAX_SCHEDULE_EVENTS = 32;
const SCHEDULE_SAVE_TIMEOUT_MS = 60000;
const DAY_NAMES = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

// Local hosting uses a separate demo project: never talk to the live clock.
const localTesting = ['localhost', '127.0.0.1'].includes(window.location.hostname);
const localConfig = {
  apiKey: 'demo-api-key',
  authDomain: 'demo-smart-shabbat-clock.firebaseapp.com',
  databaseURL: 'https://demo-smart-shabbat-clock-default-rtdb.firebaseio.com',
  projectId: 'demo-smart-shabbat-clock',
  appId: 'demo-smart-shabbat-clock-web'
};
const app = initializeApp(localTesting ? localConfig : firebaseConfig);
const auth = getAuth(app);
const db = getDatabase(app);
if (localTesting) {
  connectAuthEmulator(auth, 'http://127.0.0.1:9099', { disableWarnings: true });
  connectDatabaseEmulator(db, '127.0.0.1', 9000);
}

const authPanel = document.getElementById('authPanel');
const controlPanel = document.getElementById('controlPanel');
const signInForm = document.getElementById('signInForm');
const signOutButton = document.getElementById('signOutButton');
const connectionLabel = document.getElementById('connectionLabel');
const message = document.getElementById('message');
const scheduleList = document.getElementById('scheduleList');
const scheduleDialog = document.getElementById('scheduleDialog');
const scheduleEditorForm = document.getElementById('scheduleEditorForm');
const scheduleEditorList = document.getElementById('scheduleEditorList');
const scheduleEditorError = document.getElementById('scheduleEditorError');
const scheduleEditorNotice = document.getElementById('scheduleEditorNotice');
const scheduleEditorWarning = document.getElementById('scheduleEditorWarning');
const scheduleDay = document.getElementById('scheduleDay');
const scheduleHour = document.getElementById('scheduleHour');
const scheduleMinute = document.getElementById('scheduleMinute');
const scheduleState = document.getElementById('scheduleState');
const editScheduleButton = document.getElementById('editScheduleButton');
const closeScheduleEditorButton = document.getElementById('closeScheduleEditorButton');
const cancelScheduleEditorButton = document.getElementById('cancelScheduleEditorButton');
const saveScheduleButton = document.getElementById('saveScheduleButton');
const accessPanel = document.getElementById('accessPanel');
const accessStatus = document.getElementById('accessStatus');
const conferencePanel = document.getElementById('conferencePanel');
const conferenceStatus = document.getElementById('conferenceStatus');
const conferenceToggleButton = document.getElementById('conferenceToggleButton');
const conferenceJoinLink = document.getElementById('conferenceJoinLink');
const conferenceLinkBox = document.getElementById('conferenceLinkBox');
const copyJoinLinkButton = document.getElementById('copyJoinLinkButton');
const isQrEntry = window.location.pathname.replace(/\/$/, '') === '/join';

let currentUser = null;
let currentStatus = {};
let currentSchedule = { revision: 0, events: [] };
let firebaseSchedule = { revision: null, events: [] };
let scheduleDraft = [];
let scheduleLoaded = false;
let pendingScheduleSave = null;
let lastCommandUnsubscribe = null;
let statusUnsubscribe = null;
let scheduleUnsubscribe = null;
let roleUnsubscribe = null;
let activeVisitorUnsubscribe = null;
let conferenceEnabled = false;
let conferenceJoinCode = null;
let conferenceCodeUnsubscribe = null;
let conferenceKnown = false;
let activeVisitorUid = null;
let activeVisitorKnown = false;
let adminApproved = false;
let editorBusy = false;
let joinStarted = false;
let joinInProgress = false;
let conferenceToggleBusy = false;
let hadControl = false;
let signOutGuestInProgress = false;

const fields = {
  stateSummary: document.getElementById('stateSummary'),
  nextEventSummary: document.getElementById('nextEventSummary'),
  currentTimeSummary: document.getElementById('currentTimeSummary'),
  timeStatusBadge: document.getElementById('timeStatusBadge'),
  hc12StatusBadge: document.getElementById('hc12StatusBadge'),
  lastSeq: document.getElementById('lastSeqStatus'),
  scheduleRevision: document.getElementById('scheduleRevisionStatus'),
  firebaseScheduleRevision: document.getElementById('firebaseScheduleRevisionStatus'),
  lastSeen: document.getElementById('lastSeen'),
  scheduleCount: document.getElementById('scheduleCount'),
  scheduleSyncWarning: document.getElementById('scheduleSyncWarning'),
  lastCommandId: document.getElementById('lastCommandId'),
  lastCommandSeq: document.getElementById('lastCommandSeq'),
  lastCommandType: document.getElementById('lastCommandType'),
  ackStatus: document.getElementById('ackStatus'),
  debugUser: document.getElementById('debugUser'),
  debugDevice: document.getElementById('debugDevice'),
  debugProject: document.getElementById('debugProject'),
  debugLastError: document.getElementById('debugLastError'),
  scheduleSaveRevisionBefore: document.getElementById('scheduleSaveRevisionBefore'),
  scheduleSaveRevisionAfter: document.getElementById('scheduleSaveRevisionAfter')
};

fields.debugDevice.textContent = DEVICE_ID;
fields.debugProject.textContent = app.options.projectId || '--';

function setMessage(text, isError = false, detail = '') {
  message.textContent = detail ? `${text}\nDetails: ${detail}` : text;
  message.hidden = !text;
  message.classList.toggle('error', isError);
}

function setError(context, error) {
  const detail = error?.message || String(error || 'Unknown error');
  fields.debugLastError.textContent = `${context}: ${detail}`;
  fields.debugLastError.classList.add('has-error');
  setMessage(`${context}: ${detail}`, true);
  console.error(context, error);
}

// The database rules provide the final decision on every write.
function canControl() {
  if (!currentUser) return false;
  if (adminApproved) return true;
  return currentUser.isAnonymous && conferenceKnown && conferenceEnabled &&
    activeVisitorKnown && activeVisitorUid === currentUser.uid;
}

function isGuest() {
  return currentUser !== null && currentUser.isAnonymous;
}

function updateAccessUI() {
  const allowed = canControl();
  const guest = isGuest();
  const hasAccess = Boolean(currentUser) && (adminApproved || (guest && conferenceEnabled));

  authPanel.hidden = Boolean(currentUser) || joinInProgress;
  controlPanel.hidden = !hasAccess;
  signOutButton.hidden = !currentUser;
  accessPanel.hidden = !currentUser;
  conferencePanel.hidden = !adminApproved;
  conferenceToggleButton.disabled = conferenceToggleBusy || !conferenceKnown;
  conferenceToggleButton.textContent = conferenceEnabled ? 'End conference' : 'Start conference';
  conferenceStatus.textContent = conferenceEnabled ? 'Open' : 'Closed';
  const link = conferenceEnabled && conferenceJoinCode
    ? `${window.location.origin}/join?code=${encodeURIComponent(conferenceJoinCode)}` : '';
  conferenceLinkBox.hidden = !link;
  conferenceJoinLink.value = link;
  copyJoinLinkButton.disabled = !link;

  if (adminApproved) {
    accessStatus.textContent = 'Administrator: full control';
  } else if (guest && !conferenceEnabled) {
    accessStatus.textContent = 'Conference is closed. Guest access has ended.';
  } else if (guest && allowed) {
    accessStatus.textContent = 'You have control of the clock.';
  } else if (guest) {
    accessStatus.textContent = 'View only: another visitor has control.';
  } else if (currentUser) {
    accessStatus.textContent = 'This account does not have administrator access.';
  }

  document.querySelectorAll('[data-command]').forEach((button) => {
    button.disabled = !allowed || button.dataset.sending === 'true';
  });
  editScheduleButton.disabled = !allowed || !scheduleLoaded || editorBusy;
  saveScheduleButton.disabled = !allowed || !scheduleLoaded || editorBusy;
  scheduleEditorForm.querySelectorAll('input, select, button').forEach((element) => {
    element.disabled = !allowed || editorBusy;
  });

  if (hadControl && !allowed && guest) {
    if (scheduleDialog.open) scheduleDialog.close();
    setMessage(conferenceEnabled
      ? 'A new visitor has taken control. You can still view the clock.'
      : 'The conference has ended. Guest access is revoked.');
  }
  hadControl = allowed;
}

function unsubscribeDevice() {
  if (statusUnsubscribe) statusUnsubscribe();
  if (scheduleUnsubscribe) scheduleUnsubscribe();
  if (lastCommandUnsubscribe) lastCommandUnsubscribe();
  if (activeVisitorUnsubscribe) activeVisitorUnsubscribe();
  statusUnsubscribe = null;
  scheduleUnsubscribe = null;
  lastCommandUnsubscribe = null;
  activeVisitorUnsubscribe = null;
  activeVisitorKnown = false;
  activeVisitorUid = null;
}

// Existing visitors keep their anonymous account when refreshing "/".
// Only a fresh visit through "/join" claims the controller position.
async function joinConference(initialUser) {
  joinInProgress = true;
  updateAccessUI();
  try {
    const scanCode = new URLSearchParams(window.location.search).get('code');
    if (!scanCode || !/^[0-9a-f]{64}$/.test(scanCode)) {
      throw new Error('Invalid QR link. Scan the current conference code.');
    }
    const enabled = await get(ref(db, '/conference/enabled'));
    if (enabled.val() !== true) throw new Error('Conference is not currently open');

    if (initialUser && !initialUser.isAnonymous) {
      setMessage('Already signed in as an administrator.');
      return;
    }

    // A new scan gets a new anonymous Firebase UID, even in the same browser.
    if (auth.currentUser) await signOut(auth);
    const result = await signInAnonymously(auth);
    // Atomic claim: rules check this QR code against the private admin code.
    // The visitor's UID is visible to other visitors; the QR code is not.
    await set(ref(db, '/conference/activeVisitor'), {
      uid: result.user.uid,
      code: scanCode
    });
    setMessage('You have control of the clock.');
  } catch (error) {
    setError('Conference sign-in failed', error);
    if (auth.currentUser?.isAnonymous) await signOut(auth).catch(() => {});
  } finally {
    // Prevent refreshing the scan URL from taking control again.
    window.history.replaceState(null, '', '/');
    joinInProgress = false;
    updateAccessUI();
  }
}

async function revokeGuestIfClosed() {
  if (!conferenceKnown || conferenceEnabled || !isGuest() || joinInProgress || signOutGuestInProgress) return;
  signOutGuestInProgress = true;
  try {
    await signOut(auth);
    setMessage('The conference has ended. Please use an administrator account to sign in.');
  } catch (error) {
    setError('Could not sign out conference guest', error);
  } finally {
    signOutGuestInProgress = false;
  }
}

function subscribeDeviceIfAllowed() {
  unsubscribeDevice();
  if (!currentUser || !(adminApproved || (isGuest() && conferenceEnabled))) {
    updateAccessUI();
    return;
  }

  activeVisitorUnsubscribe = onValue(ref(db, '/conference/activeVisitor/uid'), (snapshot) => {
    activeVisitorKnown = true;
    activeVisitorUid = snapshot.val();
    updateAccessUI();
  }, (error) => {
    if (conferenceEnabled || adminApproved) setError('Cannot read conference control', error);
  });

  statusUnsubscribe = onValue(ref(db, `/devices/${DEVICE_ID}/state/status`), (snapshot) => {
    renderStatus(snapshot.val());
  }, (error) => setError(`Cannot read /devices/${DEVICE_ID}/state/status`, error));

  scheduleUnsubscribe = onValue(ref(db, `/devices/${DEVICE_ID}/state/schedule`), (snapshot) => {
    renderSchedule(snapshot.val());
    setScheduleEditorBusy(false);
  }, (error) => setError(`Cannot read /devices/${DEVICE_ID}/state/schedule`, error));
  updateAccessUI();
}

// "enabled" is public only so the QR landing page can check the event.
onValue(ref(db, '/conference/enabled'), (snapshot) => {
  const nextEnabled = snapshot.val() === true;
  const changed = !conferenceKnown || conferenceEnabled !== nextEnabled;
  conferenceKnown = true;
  conferenceEnabled = nextEnabled;
  if (changed) subscribeDeviceIfAllowed();
  updateAccessUI();
  revokeGuestIfClosed();
}, (error) => setError('Cannot check conference mode', error));

// Only administrators can read the conference admission code.
function subscribeConferenceCode() {
  if (conferenceCodeUnsubscribe) conferenceCodeUnsubscribe();
  conferenceCodeUnsubscribe = null;
  conferenceJoinCode = null;
  if (!adminApproved) {
    updateAccessUI();
    return;
  }
  conferenceCodeUnsubscribe = onValue(ref(db, '/conference/joinCode'), (snapshot) => {
    conferenceJoinCode = snapshot.val();
    updateAccessUI();
  }, (error) => setError('Cannot read conference QR link', error));
}

conferenceToggleButton.addEventListener('click', async () => {
  if (!adminApproved || conferenceToggleBusy) return;
  const open = !conferenceEnabled;
  conferenceToggleBusy = true;
  updateAccessUI();
  try {
    // A fresh 256-bit QR code is generated every time the conference opens.
    const code = open
      ? Array.from(crypto.getRandomValues(new Uint8Array(32)),
          (byte) => byte.toString(16).padStart(2, '0')).join('')
      : null;
    await update(ref(db, '/conference'), {
      enabled: open,
      activeVisitor: null,
      joinCode: code
    });
    setMessage(open
      ? 'Conference opened. Copy the new QR link below.'
      : 'Conference ended. Guest access and previous QR links are revoked.');
  } catch (error) {
    setError('Could not change conference mode', error);
  } finally {
    conferenceToggleBusy = false;
    updateAccessUI();
  }
});

copyJoinLinkButton.addEventListener('click', async () => {
  if (!adminApproved || !conferenceJoinLink.value) return;
  try {
    await navigator.clipboard.writeText(conferenceJoinLink.value);
    setMessage('Conference link copied. Generate the printed QR from this URL.');
  } catch (error) {
    setError('Cannot copy conference link', error);
  }
});

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

function setBadge(element, text, variant = '') {
  element.textContent = text;
  element.className = `value-badge ${variant}`.trim();
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

function normalizeScheduleEvent(event) {
  const normalized = {
    day: Number(event?.day),
    hour: Number(event?.hour),
    minute: Number(event?.minute),
    state: event?.state
  };

  if (!Number.isInteger(normalized.day) || normalized.day < 0 || normalized.day > 6) {
    throw new Error('Schedule day must be from 0 to 6');
  }
  if (!Number.isInteger(normalized.hour) || normalized.hour < 0 || normalized.hour > 23) {
    throw new Error('Schedule hour must be from 0 to 23');
  }
  if (!Number.isInteger(normalized.minute) || normalized.minute < 0 || normalized.minute > 59) {
    throw new Error('Schedule minute must be from 0 to 59');
  }
  if (normalized.state !== 'on' && normalized.state !== 'off') {
    throw new Error('Schedule state must be on or off');
  }

  return normalized;
}

function compareScheduleEvents(a, b) {
  return a.day - b.day || a.hour - b.hour || a.minute - b.minute;
}

function prepareScheduleEvents(events) {
  if (!Array.isArray(events)) throw new Error('Schedule events must be a list');
  if (events.length > MAX_SCHEDULE_EVENTS) {
    throw new Error(`Schedule cannot contain more than ${MAX_SCHEDULE_EVENTS} events`);
  }
  return normalizeWeeklyEvents(events).events;
}

function validateScheduleEvents(events) {
  if (!Array.isArray(events)) throw new Error('Schedule events must be a list');
  if (events.length > MAX_SCHEDULE_EVENTS) {
    throw new Error(`Schedule cannot contain more than ${MAX_SCHEDULE_EVENTS} events`);
  }
  return events.map(normalizeScheduleEvent).sort(compareScheduleEvents);
}

function minuteOfWeek(event) {
  return event.day * 1440 + event.hour * 60 + event.minute;
}

function sameScheduleSlot(a, b) {
  return a.day === b.day && a.hour === b.hour && a.minute === b.minute;
}

function eventSignature(event) {
  return `${event.day}:${event.hour}:${event.minute}:${event.state}`;
}

function countEventSignatures(events) {
  return events.reduce((counts, event) => {
    const signature = eventSignature(event);
    counts.set(signature, (counts.get(signature) || 0) + 1);
    return counts;
  }, new Map());
}

function removedByNormalization(before, after) {
  const remaining = countEventSignatures(after);
  return before.filter((event) => {
    const signature = eventSignature(event);
    const count = remaining.get(signature) || 0;
    if (count > 0) {
      remaining.set(signature, count - 1);
      return false;
    }
    return true;
  });
}

function normalizeWeeklyEvents(events) {
  const sorted = events.map(normalizeScheduleEvent).sort(compareScheduleEvents);
  if (sorted.length <= 1) return { events: sorted, removed: [] };

  const unique = [];
  sorted.forEach((event) => {
    if (unique.length > 0 && sameScheduleSlot(unique[unique.length - 1], event)) {
      unique[unique.length - 1] = event;
    } else {
      unique.push(event);
    }
  });

  if (unique.length <= 1) {
    return { events: unique, removed: removedByNormalization(sorted, unique) };
  }

  const normalized = unique.filter((event, index) => {
    const previous = unique[index === 0 ? unique.length - 1 : index - 1];
    return event.state !== previous.state;
  });

  const eventsToKeep = normalized.length ? normalized : [unique[0]];
  return {
    events: eventsToKeep,
    removed: removedByNormalization(sorted, eventsToKeep)
  };
}

function cloneEvents(events) {
  return events.map((event) => ({ ...event }));
}

function formatEventTime(event) {
  return `${DAY_NAMES[event.day] || '--'} ${String(event.hour).padStart(2, '0')}:${String(event.minute).padStart(2, '0')}`;
}

function parseClockTime(value) {
  const match = /^(\d{1,2}):(\d{2})/.exec(String(value || ''));
  if (!match) return null;
  const hour = Number(match[1]);
  const minute = Number(match[2]);
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return null;
  return { hour, minute };
}

function findNextEvent(events) {
  if (!events.length) return null;

  const now = new Date();
  const clockTime = parseClockTime(currentStatus.time);
  const currentDay = now.getDay();
  const currentMinuteOfWeek = currentDay * 1440 + (
    clockTime ? clockTime.hour * 60 + clockTime.minute : now.getHours() * 60 + now.getMinutes()
  );

  return events
    .map((event, index) => {
      let delta = event.day * 1440 + event.hour * 60 + event.minute - currentMinuteOfWeek;
      if (delta < 0) delta += 7 * 1440;
      return { event, index, delta };
    })
    .sort((a, b) => a.delta - b.delta)[0];
}

function setActiveCommandButtons(status) {
  const relayMode = status.relayMode === 1 ? 'on' : status.relayMode === 0 ? 'off' : status.relayMode === 2 ? 'auto' : '';
  const shabbatMode = status.shabbat === true ? 'shabbat' : status.shabbat === false ? 'week' : '';

  document.querySelectorAll('[data-command="relay_mode"]').forEach((button) => {
    const isActive = button.dataset.mode === relayMode;
    button.classList.toggle('is-active', isActive);
    button.setAttribute('aria-pressed', String(isActive));
  });

  document.querySelectorAll('[data-command="shabbat_mode"]').forEach((button) => {
    const isActive = button.dataset.mode === shabbatMode;
    button.classList.toggle('is-active', isActive);
    button.setAttribute('aria-pressed', String(isActive));
  });
}

function updateOverview() {
  const relayMode = formatRelayMode(currentStatus.relayMode);
  const relayState = currentStatus.relay === true ? 'Relay ON' : currentStatus.relay === false ? 'Relay OFF' : 'Relay --';
  const shabbatState = currentStatus.shabbat === true ? 'Shabbat mode active' : currentStatus.shabbat === false ? 'Week mode active' : 'Mode --';
  fields.stateSummary.textContent = `${relayMode} mode - ${relayState} - ${shabbatState}`;
  fields.currentTimeSummary.textContent = `Current time: ${currentStatus.time || '--'}`;

  const nextEvent = findNextEvent(currentSchedule.events);
  fields.nextEventSummary.textContent = nextEvent
    ? `Next event: ${formatEventTime(nextEvent.event)} ${nextEvent.event.state.toUpperCase()}`
    : 'Next event: --';

  const timeValid = currentStatus.timeValid === true;
  const timeInvalid = currentStatus.timeValid === false;
  setBadge(fields.timeStatusBadge, `Time: ${timeValid ? 'OK' : timeInvalid ? 'Invalid' : '--'}`, timeValid ? 'is-ok' : timeInvalid ? 'is-invalid' : '');

  const hc12Ok = currentStatus.hc12Ok === true;
  const hc12Failed = currentStatus.hc12Ok === false;
  setBadge(fields.hc12StatusBadge, `HC-12: ${hc12Ok ? 'OK' : hc12Failed ? 'No response' : '--'}`, hc12Ok ? 'is-ok' : hc12Failed ? 'is-invalid' : '');

  setBadge(fields.scheduleCount, `Schedule: ${currentSchedule.events.length} event${currentSchedule.events.length === 1 ? '' : 's'}`, currentSchedule.events.length ? 'is-neutral' : '');
  fields.lastSeq.textContent = Number.isFinite(currentStatus.lastProcessedSeq) ? currentStatus.lastProcessedSeq : '--';
  fields.scheduleRevision.textContent = Number.isFinite(currentStatus.scheduleRevision) ? currentStatus.scheduleRevision : '--';
  fields.firebaseScheduleRevision.textContent = Number.isFinite(firebaseSchedule.revision) ? firebaseSchedule.revision : '--';
  setActiveCommandButtons(currentStatus);
}

function renderStatus(status) {
  currentStatus = status || {};
  fields.lastSeen.textContent = formatTimestamp(currentStatus.lastSeen);
  reconcileScheduleState();
}

function renderScheduleRows(container, events, options = {}) {
  const nextEvent = !options.editable ? findNextEvent(events) : null;
  container.innerHTML = '';

  if (events.length === 0) {
    const empty = document.createElement('p');
    empty.textContent = options.emptyText || 'No schedule published by the device yet.';
    empty.className = 'message';
    container.appendChild(empty);
    return;
  }

  events.forEach((event, index) => {
    const row = document.createElement('div');
    row.className = 'schedule-item';
    if (nextEvent?.index === index) row.classList.add('is-next');

    const when = document.createElement('strong');
    when.textContent = formatEventTime(event);

    const state = document.createElement('span');
    state.className = `schedule-state ${event.state === 'on' ? 'is-on' : 'is-off'}`;
    state.textContent = event.state;

    row.append(when, state);

    if (nextEvent?.index === index) {
      const nextBadge = document.createElement('span');
      nextBadge.className = 'next-badge';
      nextBadge.textContent = 'Next';
      row.appendChild(nextBadge);
    }

    if (options.editable) {
      const deleteButton = document.createElement('button');
      deleteButton.type = 'button';
      deleteButton.className = 'delete-schedule-button';
      deleteButton.textContent = 'Delete';
      deleteButton.setAttribute('aria-label', `Delete ${formatEventTime(event)} ${event.state} event`);
      deleteButton.addEventListener('click', () => {
        scheduleDraft.splice(index, 1);
        setScheduleEditorNotice('');
        renderScheduleDraft();
      });
      row.appendChild(deleteButton);
    }

    container.appendChild(row);
  });
}

function renderSchedulePlaceholder(text) {
  scheduleList.innerHTML = '';
  const placeholder = document.createElement('p');
  placeholder.textContent = text;
  placeholder.className = 'message';
  scheduleList.appendChild(placeholder);
}

function setScheduleSyncWarning(text) {
  fields.scheduleSyncWarning.textContent = text;
  fields.scheduleSyncWarning.hidden = !text;
}

function confirmedScheduleRevision() {
  return Number.isFinite(currentStatus.scheduleRevision) ? currentStatus.scheduleRevision : null;
}

function reconcileScheduleState() {
  const deviceRevision = confirmedScheduleRevision();
  const firebaseRevision = Number.isFinite(firebaseSchedule.revision) ? firebaseSchedule.revision : null;

  if (deviceRevision === null || firebaseRevision === null) {
    scheduleLoaded = false;
    currentSchedule = { revision: deviceRevision ?? 0, events: [] };
    setScheduleSyncWarning('Waiting for device schedule status...');
    renderSchedulePlaceholder('Waiting for device schedule status...');
    setScheduleEditorBusy(false);
    updateOverview();
    return;
  }

  if (deviceRevision !== firebaseRevision) {
    scheduleLoaded = false;
    currentSchedule = { revision: deviceRevision, events: [] };
    setScheduleSyncWarning('Schedule is out of sync with the device. Waiting for device refresh...');
    renderSchedulePlaceholder('Schedule is out of sync with the device. Waiting for device refresh...');
    setScheduleEditorBusy(false);
    updateOverview();
    return;
  }

  scheduleLoaded = true;
  currentSchedule = {
    revision: firebaseRevision,
    events: firebaseSchedule.events
  };
  setScheduleSyncWarning('');
  renderScheduleRows(scheduleList, currentSchedule.events);
  noteScheduleRefresh(currentSchedule.revision);
  setScheduleEditorBusy(false);
  updateOverview();
}

function renderSchedule(schedule) {
  let events;
  try {
    events = Number(schedule?.eventCount) === 0 || !schedule?.events
      ? []
      : validateScheduleEvents(normalizeEvents(schedule.events));
  } catch (error) {
    events = [];
    setError('Invalid schedule received from device', error);
  }

  firebaseSchedule = {
    revision: Number.isFinite(schedule?.revision) ? schedule.revision : 0,
    events
  };

  reconcileScheduleState();
}

function buildPayload(type, mode, scheduleEvents = currentSchedule.events) {
  if (type === 'relay_mode') return { mode };
  if (type === 'shabbat_mode') return { mode };
  if (type === 'replace_schedule') {
    if (!scheduleLoaded) throw new Error('Wait for the current schedule to load');
    const events = prepareScheduleEvents(scheduleEvents);
    const payload = {
      baseScheduleRevision: currentSchedule.revision,
      eventCount: events.length
    };
    if (events.length === 0) {
      return payload;
    }
    payload.events = events;
    return payload;
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

function commandSuccessText(type, mode) {
  if (type === 'replace_schedule') return 'Command completed: schedule updated';
  if (type === 'relay_mode') return `Command completed: relay ${mode}`;
  if (type === 'shabbat_mode') return `Command completed: ${mode} mode`;
  return 'Command completed';
}

function watchAck(commandId, type, mode) {
  if (lastCommandUnsubscribe) lastCommandUnsubscribe();
  setBadge(fields.ackStatus, 'Waiting', 'is-waiting');

  return new Promise((resolve, reject) => {
    lastCommandUnsubscribe = onValue(ref(db, `/devices/${DEVICE_ID}/acks/${commandId}`), (snapshot) => {
      if (!snapshot.exists()) return;

      const ack = snapshot.val();
      const detail = ack.code || (ack.ok ? 'ok' : 'failed');
      setBadge(fields.ackStatus, detail, ack.ok ? 'is-ok' : 'is-failed');

      if (lastCommandUnsubscribe) {
        const unsubscribe = lastCommandUnsubscribe;
        lastCommandUnsubscribe = null;
        unsubscribe();
      }

      if (ack.ok) {
        setMessage(type === 'replace_schedule' ? 'Waiting for device schedule refresh...' : commandSuccessText(type, mode));
        fields.debugLastError.textContent = 'None';
        fields.debugLastError.classList.remove('has-error');
        resolve(ack);
        return;
      }

      const text = ack.message ? `Command failed: ${ack.message}` : 'Command failed';
      fields.debugLastError.textContent = `${text}: ${detail}`;
      fields.debugLastError.classList.add('has-error');
      setMessage(text, true, detail);
      reject(new Error(`${detail}${ack.message ? `: ${ack.message}` : ''}`));
    }, (error) => {
      setError(`Cannot read command result at /devices/${DEVICE_ID}/acks/${commandId}`, error);
      reject(error);
    });
  });
}

async function sendCommand(type, mode = undefined, scheduleEvents = undefined) {
  if (!canControl()) throw new Error('You do not have control of the clock');

  const seq = await nextCommandSeq();
  if (!canControl()) throw new Error('Control was transferred to another visitor');
  const commandId = `c${String(seq).padStart(9, '0')}`;
  const command = {
    seq,
    type,
    payload: buildPayload(type, mode, scheduleEvents),
    createdBy: currentUser.uid,
    guest: isGuest(),
    createdAt: serverTimestamp()
  };

  console.log('Outgoing command', JSON.stringify(command, null, 2));
  await set(ref(db, `/devices/${DEVICE_ID}/commands/${commandId}`), command);

  fields.lastCommandId.textContent = commandId;
  fields.lastCommandSeq.textContent = seq;
  fields.lastCommandType.textContent = type;
  fields.debugLastError.textContent = 'None';
  fields.debugLastError.classList.remove('has-error');
  const ackPromise = watchAck(commandId, type, mode);
  ackPromise.catch(() => {});
  setMessage('Command sent; waiting for device result.');
  return { commandId, ackPromise };
}

function setScheduleEditorError(text) {
  scheduleEditorError.textContent = text;
  scheduleEditorError.hidden = !text;
}

function setScheduleEditorNotice(text) {
  scheduleEditorNotice.textContent = text;
  scheduleEditorNotice.hidden = !text;
}

function setScheduleEditorWarning(text) {
  scheduleEditorWarning.textContent = text;
  scheduleEditorWarning.hidden = !text;
}

function clearPendingScheduleSave() {
  if (!pendingScheduleSave) return;
  window.clearTimeout(pendingScheduleSave.timeoutId);
  pendingScheduleSave = null;
}

function maybeCompletePendingScheduleSave() {
  if (!pendingScheduleSave || !pendingScheduleSave.ackOk || !pendingScheduleSave.scheduleRefreshed) return;

  const pending = pendingScheduleSave;
  const afterRevision = currentSchedule.revision;
  fields.scheduleSaveRevisionAfter.textContent = afterRevision;
  setScheduleEditorNotice(`Schedule saved by device. Revision ${pending.baseRevision} -> ${afterRevision}.`);
  setMessage('Schedule saved by device');
  clearPendingScheduleSave();
  setScheduleEditorBusy(false);
  closeScheduleEditor();
  pending.resolve();
}

function noteScheduleRefresh(revision) {
  if (!pendingScheduleSave) return;
  if (!Number.isFinite(revision) || revision <= pendingScheduleSave.baseRevision) return;

  pendingScheduleSave.scheduleRefreshed = true;
  setScheduleEditorNotice('Schedule saved by device. Updating cloud view from Firebase...');
  maybeCompletePendingScheduleSave();
}

function waitForDeviceScheduleSave(commandId, baseRevision, ackPromise) {
  return new Promise((resolve, reject) => {
    clearPendingScheduleSave();

    pendingScheduleSave = {
      commandId,
      baseRevision,
      ackOk: false,
      scheduleRefreshed: Number.isFinite(currentSchedule.revision) && currentSchedule.revision > baseRevision,
      resolve,
      reject,
      timeoutId: window.setTimeout(() => {
        if (!pendingScheduleSave || pendingScheduleSave.commandId !== commandId) return;
        const pending = pendingScheduleSave;
        pendingScheduleSave = null;
        if (lastCommandUnsubscribe) {
          const unsubscribe = lastCommandUnsubscribe;
          lastCommandUnsubscribe = null;
          unsubscribe();
        }
        setBadge(fields.ackStatus, 'timeout', 'is-failed');
        pending.reject(new Error('Timed out waiting for device to confirm and publish the schedule'));
      }, SCHEDULE_SAVE_TIMEOUT_MS)
    };

    ackPromise
      .then(() => {
        if (!pendingScheduleSave || pendingScheduleSave.commandId !== commandId) return;
        pendingScheduleSave.ackOk = true;
        setScheduleEditorNotice('Waiting for device to confirm schedule...');
        maybeCompletePendingScheduleSave();
      })
      .catch((error) => {
        if (!pendingScheduleSave || pendingScheduleSave.commandId !== commandId) return;
        clearPendingScheduleSave();
        reject(error);
      });
  });
}

function setScheduleEditorBusy(busy) {
  editorBusy = busy;
  updateAccessUI();
}

function findPreviousEffectiveEvent(events, event) {
  if (events.length === 0) return null;

  const targetMinute = minuteOfWeek(event);
  let previous = null;
  events.forEach((entry) => {
    const entryMinute = minuteOfWeek(entry);
    if (entryMinute >= targetMinute) return;
    if (!previous || entryMinute > minuteOfWeek(previous)) previous = entry;
  });

  if (previous) return previous;
  return events.reduce((latest, entry) => minuteOfWeek(entry) > minuteOfWeek(latest) ? entry : latest, events[0]);
}

function formatEventLabel(event) {
  return `${formatEventTime(event)} ${event.state.toUpperCase()}`;
}

function longOnWarning(events) {
  if (events.length === 0) return '';
  if (events.length === 1 && events[0].state === 'on') {
    return 'This keeps the relay ON for the whole week. Consider adding an OFF event.';
  }

  for (let index = 0; index < events.length; index++) {
    const event = events[index];
    if (event.state !== 'on') continue;

    const next = events[(index + 1) % events.length];
    let duration = minuteOfWeek(next) - minuteOfWeek(event);
    if (duration <= 0) duration += 7 * 1440;

    if (duration > 1440 && next.state === 'off') {
      return `This keeps the relay ON until ${formatEventTime(next)} OFF. Consider adding an OFF event.`;
    }
  }

  return '';
}

function renderScheduleDraft() {
  scheduleDraft = normalizeWeeklyEvents(scheduleDraft).events;
  renderScheduleRows(scheduleEditorList, scheduleDraft, {
    editable: true,
    emptyText: 'No events in this draft.'
  });
  setScheduleEditorWarning(longOnWarning(scheduleDraft));
}

function openScheduleEditor() {
  if (!canControl()) {
    setMessage('This session is view only.', true);
    return;
  }
  if (!scheduleLoaded) {
    setMessage('Schedule is out of sync with the device. Waiting for device refresh...', true);
    return;
  }

  scheduleDraft = cloneEvents(currentSchedule.events);
  setScheduleEditorError('');
  setScheduleEditorNotice('');
  renderScheduleDraft();

  if (typeof scheduleDialog.showModal === 'function') {
    scheduleDialog.showModal();
  } else {
    scheduleDialog.setAttribute('open', '');
  }
}

function closeScheduleEditor() {
  scheduleDialog.close();
}

function addScheduleDraftEvent() {
  if (!canControl()) throw new Error('This session is view only');
  const newEvent = normalizeScheduleEvent({
    day: Number(scheduleDay.value),
    hour: scheduleHour.valueAsNumber,
    minute: scheduleMinute.valueAsNumber,
    state: scheduleState.value
  });

  const existingIndex = scheduleDraft.findIndex((entry) =>
    entry.day === newEvent.day &&
    entry.hour === newEvent.hour &&
    entry.minute === newEvent.minute
  );

  if (existingIndex >= 0) {
    if (scheduleDraft[existingIndex].state === newEvent.state) {
      throw new Error('An identical event already exists at that day and time');
    }
    const proposedEvents = cloneEvents(scheduleDraft);
    proposedEvents[existingIndex] = newEvent;
    const normalized = normalizeWeeklyEvents(proposedEvents);
    scheduleDraft = normalized.events;
    if (normalized.removed.length > 0) {
      setScheduleEditorNotice(`Removed redundant event${normalized.removed.length === 1 ? '' : 's'}: ${normalized.removed.map(formatEventLabel).join(', ')}.`);
    } else {
      setScheduleEditorNotice('');
    }
  } else {
    if (scheduleDraft.length >= MAX_SCHEDULE_EVENTS) {
      throw new Error(`Schedule cannot contain more than ${MAX_SCHEDULE_EVENTS} events`);
    }

    const previous = findPreviousEffectiveEvent(scheduleDraft, newEvent);
    if (previous && previous.state === newEvent.state) {
      throw new Error(`No change - previous effective event is already ${newEvent.state.toUpperCase()} at ${formatEventTime(previous)}`);
    }

    const proposedEvents = [...scheduleDraft, newEvent];
    const normalized = normalizeWeeklyEvents(proposedEvents);
    scheduleDraft = normalized.events;
    if (normalized.removed.length > 0) {
      setScheduleEditorNotice(`Removed redundant event${normalized.removed.length === 1 ? '' : 's'}: ${normalized.removed.map(formatEventLabel).join(', ')}.`);
    } else {
      setScheduleEditorNotice('');
    }
  }

  renderScheduleDraft();
}

async function saveScheduleDraft() {
  if (!canControl()) throw new Error('This session is view only');
  const preparedEvents = prepareScheduleEvents(scheduleDraft);
  const baseRevision = currentSchedule.revision;
  fields.scheduleSaveRevisionBefore.textContent = baseRevision;
  fields.scheduleSaveRevisionAfter.textContent = '--';
  setScheduleEditorNotice('Saving schedule...');
  setMessage('Saving schedule...');

  const command = await sendCommand('replace_schedule', undefined, preparedEvents);
  setScheduleEditorNotice('Waiting for device to confirm schedule...');
  await waitForDeviceScheduleSave(command.commandId, baseRevision, command.ackPromise);
}

signInForm.addEventListener('submit', async (event) => {
  event.preventDefault();
  setMessage('');
  const email = document.getElementById('emailInput').value;
  const password = document.getElementById('passwordInput').value;

  try {
    await signInWithEmailAndPassword(auth, email, password);
  } catch (error) {
    setError('Sign in failed', error);
  }
});

signOutButton.addEventListener('click', () => {
  signOut(auth).catch((error) => setError('Sign out failed', error));
});

document.querySelectorAll('[data-command]').forEach((button) => {
  button.addEventListener('click', async () => {
    setMessage('');
    button.dataset.sending = 'true';
    updateAccessUI();
    try {
      await sendCommand(button.dataset.command, button.dataset.mode);
    } catch (error) {
      setError(`${button.textContent.trim()} command failed`, error);
    } finally {
      delete button.dataset.sending;
      updateAccessUI();
    }
  });
});

editScheduleButton.addEventListener('click', openScheduleEditor);
closeScheduleEditorButton.addEventListener('click', closeScheduleEditor);
cancelScheduleEditorButton.addEventListener('click', closeScheduleEditor);

scheduleEditorForm.addEventListener('submit', (event) => {
  event.preventDefault();
  setScheduleEditorError('');
  setScheduleEditorNotice('');

  try {
    addScheduleDraftEvent();
  } catch (error) {
    setScheduleEditorError(error?.message || String(error));
  }
});

saveScheduleButton.addEventListener('click', async () => {
  setMessage('');
  setScheduleEditorError('');
  setScheduleEditorBusy(true);

  try {
    await saveScheduleDraft();
  } catch (error) {
    const rawDetail = error?.message || String(error);
    const isStaleSchedule = rawDetail.includes('stale_schedule');
    const detail = isStaleSchedule
      ? 'Schedule changed on the device. Please refresh and try again.'
      : rawDetail;
    setScheduleEditorError(detail);
    fields.debugLastError.textContent = `Schedule save failed: ${rawDetail}`;
    fields.debugLastError.classList.add('has-error');
    setMessage(detail, true);
  } finally {
    setScheduleEditorBusy(false);
  }
});

onAuthStateChanged(auth, (user) => {
  currentUser = user;
  adminApproved = false;
  hadControl = false;
  scheduleLoaded = false;
  clearPendingScheduleSave();
  setScheduleEditorBusy(true);
  unsubscribeDevice();
  if (roleUnsubscribe) roleUnsubscribe();
  if (conferenceCodeUnsubscribe) conferenceCodeUnsubscribe();
  roleUnsubscribe = null;
  conferenceCodeUnsubscribe = null;
  conferenceJoinCode = null;

  fields.debugUser.textContent = user?.email || (user?.isAnonymous ? 'Conference guest' : '--');
  connectionLabel.textContent = user?.isAnonymous ? 'Conference Guest' : 'Cloud Control';

  if (user && !user.isAnonymous) {
    // Verify admin role from the database, never from email alone.
    roleUnsubscribe = onValue(ref(db, `/roles/admins/${user.uid}`), (snapshot) => {
      adminApproved = snapshot.val() === true;
      subscribeConferenceCode();
      subscribeDeviceIfAllowed();
      updateAccessUI();
      if (!adminApproved) setMessage('This account is not an administrator.', true);
    }, (error) => setError('Cannot verify administrator role', error));
  } else {
    subscribeDeviceIfAllowed();
  }

  updateAccessUI();
  revokeGuestIfClosed();

  if (isQrEntry && !joinStarted) {
    joinStarted = true;
    joinConference(user);
  }
});
