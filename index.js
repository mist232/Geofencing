const express = require('express');
const mongoose = require('mongoose');
const bodyParser = require('body-parser');
const fetch = require('node-fetch');
const MONGODB_URI="mongodb+srv://<Geofence>:<Geofence>@cluster0.qr2sq2r.mongodb.net/?retryWrites=true&w=majority&appName=Cluster0"
const app = express();
app.use(bodyParser.json());

mongoose.connect(process.env.MONGODB_URI, { useNewUrlParser: true, useUnifiedTopology: true });

const LocationSchema = new mongoose.Schema({
  device_id: String,
  timestamp: Date,
  latitude: Number,
  longitude: Number
});

const AggregatedDataSchema = new mongoose.Schema({
  device_id: String,
  location: String,
  frequency: Number,
  total_duration: Number,
  is_safe: Boolean
});

const Location = mongoose.model('Location', LocationSchema);
const AggregatedData = mongoose.model('AggregatedData', AggregatedDataSchema);

app.post('/locations', async (req, res) => {
  const { device_id, timestamp, latitude, longitude } = req.body;

  // Check if the location is within a safe zone using ML model
  const response = await fetch('https://your-model-endpoint', {
    method: 'POST',
    body: JSON.stringify({ latitude, longitude }),
    headers: { 'Content-Type': 'application/json' }
  });

  const result = await response.json();
  if (result.is_safe) {
    return res.status(200).send('Location is in a safe zone');
  }

  // Process the location data as it is not in a safe zone
  const key = `${latitude},${longitude}`;
  const existingEntry = await AggregatedData.findOne({ device_id, location: key });

  if (existingEntry) {
    // Update frequency and total duration
    const newFrequency = existingEntry.frequency + 1;
    const newTotalDuration = existingEntry.total_duration + calculateDuration(timestamp, existingEntry.timestamp);

    if (newFrequency > 5) {
      // Move to Location collection if frequency exceeds 5
      await new Location({ device_id, timestamp, latitude, longitude }).save();
      await AggregatedData.deleteOne({ _id: existingEntry._id });
      return res.status(200).send('Entry moved to Location collection');
    } else {
      await AggregatedData.updateOne(
        { _id: existingEntry._id },
        { frequency: newFrequency, total_duration: newTotalDuration, timestamp: timestamp }
      );
      return res.status(200).send('Entry updated');
    }
  } else {
    // Save new location and new aggregated data
    await new Location({ device_id, timestamp, latitude, longitude }).save();
    await new AggregatedData({
      device_id,
      location: key,
      frequency: 1,
      total_duration: calculateDuration(timestamp, timestamp),
      is_safe: result.is_safe // Initial value from model result
    }).save();
    return res.status(201).send('New entry added');
  }
});

app.get('/aggregate', async (req, res) => {
  // Your aggregation logic here
  res.status(200).send("Aggregated Data Updated");
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server is running on port ${PORT}`);
});

function calculateDuration(newTimestamp, oldTimestamp) {
  // Calculate the duration between two timestamps
  return Math.abs(new Date(newTimestamp) - new Date(oldTimestamp)) / 60000; // Duration in minutes
}
