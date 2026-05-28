# E-Waste Sorter Project Proposal Context

## Overview
An embedded systems final project designed to automatically sort mixed batteries (Li-ion, Alkaline, NiMH) into separate bins using IR sensors and servos. The system integrates hardware (ESP32) with a web-based Vue.js dashboard for real-time monitoring and remote control via MQTT and Firebase.

## Technology Stack
- **Hardware:** ESP32, IR sensors, Servos
- **Frontend Dashboard:** Vue.js, Vite, Chart.js / vue-chartjs
- **Messaging:** MQTT (HiveMQ Cloud) via WebSockets
- **Database & Auth:** Firebase Realtime Database, Firebase Authentication (Simple login overlay), Firebase Hosting

## Timeline & Milestones
- **Week 5:** Set up HiveMQ MQTT broker; implement ESP32 MQTT publish; build Firebase Realtime Database schema; connect MQTT subscriber to Firebase writer.
- **Week 6:** Develop Vue.js dashboard which is a live event table, bar charts, voltage trend chart; integrate Firebase SDK and MQTT WebSocket subscription. *(Current Phase)*
- **Week 7:** Add Firebase Authentication; implement MQTT-based remote belt control; deploy frontend to Firebase Hosting; conduct integrated system testing.
- **Week 8:** Final stress testing with 50+ batteries; document accuracy results; prepare demo video and final report; submission.

## Success Criteria
1. **Classification Accuracy ≥ 90%:** In a blind test of 50 mixed batteries (minimum 10 per chemistry type), the system correctly identifies the chemistry at least 45 out of 50 times.
2. **Sort Cycle Time ≤ 5 seconds:** From the moment the IR sensor detects a battery to the moment the servo completes the divert action, the full cycle completes within 5 seconds.
3. **MQTT Latency ≤ 2 seconds:** Every classification event is published to the MQTT broker and appears on the Vue.js dashboard within 2 seconds of the sort action completing.
4. **Remote Control Functional:** The dashboard belt toggle reliably starts and stops the conveyor within 3 seconds of user action with no observed missed commands in 10 consecutive tests.
5. **Zero False Li-ion Positives:** No Alkaline or NiMH battery is routed to the Li-ion bin (Bin 1). This is the highest-priority safety criterion, as Li-ion cells require special handling.
