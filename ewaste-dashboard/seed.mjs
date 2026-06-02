/**
 * Seed script — populates battery_events in Firebase RTDB with realistic test data.
 * Run from the ewaste-dashboard directory:
 *   node seed.mjs <email> <password> [count]
 *
 * Example:
 *   node seed.mjs bondenie@gmail.com yourpassword 40
 */

import { initializeApp } from 'firebase/app'
import { getDatabase, ref, push } from 'firebase/database'
import { getAuth, signInWithEmailAndPassword } from 'firebase/auth'

const EMAIL    = process.argv[2]
const PASSWORD = process.argv[3]
const COUNT    = parseInt(process.argv[4] ?? '30', 10)

if (!EMAIL || !PASSWORD) {
  console.error('Usage: node seed.mjs <email> <password> [count]')
  process.exit(1)
}

const firebaseConfig = {
  apiKey:            'AIzaSyCTpypQQmGy43uiavAd6FrUcph-Ro6tvsw',
  authDomain:        'embedded-final-project-29234.firebaseapp.com',
  databaseURL:       'https://embedded-final-project-29234-default-rtdb.asia-southeast1.firebasedatabase.app',
  projectId:         'embedded-final-project-29234',
  storageBucket:     'embedded-final-project-29234.firebasestorage.app',
  messagingSenderId: '859318591733',
  appId:             '1:859318591733:web:13d1ec2b45b054346da5bc'
}

const TEMPLATES = [
  { type: 'Alkaline', baseVoltage: 1.45, magnetic: false },
  { type: 'NiMH',    baseVoltage: 1.25, magnetic: true  },
  { type: 'Li-ion',  baseVoltage: 3.70, magnetic: false }
]

const jitter = (base, range) =>
  parseFloat((base + (Math.random() - 0.5) * range).toFixed(2))

function generateEvents(count) {
  const now    = Date.now()
  const weekMs = 7 * 24 * 60 * 60 * 1000
  const events = []

  for (let i = 0; i < count; i++) {
    const tpl       = TEMPLATES[i % 3]
    const timestamp = now - Math.floor(Math.random() * weekMs)
    events.push({
      type:      tpl.type,
      voltage:   jitter(tpl.baseVoltage, 0.08),
      magnetic:  tpl.magnetic,
      timestamp
    })
  }

  // oldest first so the dashboard shows them in natural order
  return events.sort((a, b) => a.timestamp - b.timestamp)
}

async function main() {
  const app  = initializeApp(firebaseConfig)
  const auth = getAuth(app)
  const db   = getDatabase(app)

  console.log(`Signing in as ${EMAIL}…`)
  await signInWithEmailAndPassword(auth, EMAIL, PASSWORD)
  console.log('Signed in.')

  const events = generateEvents(COUNT)
  console.log(`Pushing ${events.length} events to battery_events…`)

  let ok = 0
  for (const event of events) {
    try {
      await push(ref(db, 'battery_events'), event)
      process.stdout.write('.')
      ok++
    } catch (e) {
      process.stdout.write('✗')
      console.error(`\nFailed: ${e.code} — ${e.message}`)
    }
  }

  console.log(`\n\nDone — ${ok}/${events.length} events written.`)
  process.exit(0)
}

main().catch(e => {
  console.error('Fatal:', e.message)
  process.exit(1)
})
