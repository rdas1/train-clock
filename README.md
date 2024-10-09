As a hobby project and Valentine’s Day gift, I built a clock that uses an ESP32 to show the current time,
weather, and arrival times of the nearest (NYC) subway trains on an LCD display. The ESP32 periodically
requests this information from a Flask webserver I host on an AWS EC2 instance, which retrieves, cleans, and
returns data from the various relevant APIs.
