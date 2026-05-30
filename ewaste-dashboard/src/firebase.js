import { initializeApp } from 'firebase/app'
import { getDatabase } from 'firebase/database'
import { getAuth } from 'firebase/auth'

const firebaseConfig = {
  apiKey: "AIzaSyCTpypQQmGy43uiavAd6FrUcph-Ro6tvsw",
  authDomain: "embedded-final-project-29234.firebaseapp.com",
  databaseURL: "https://embedded-final-project-29234-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "embedded-final-project-29234",
  storageBucket: "embedded-final-project-29234.firebasestorage.app",
  messagingSenderId: "859318591733",
  appId: "1:859318591733:web:13d1ec2b45b054346da5bc",
  measurementId: "G-RLCE356SPC"
}

const app = initializeApp(firebaseConfig)
export const db = getDatabase(app)
export const auth = getAuth(app)
