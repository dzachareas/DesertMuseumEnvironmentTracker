import serial
import json
import threading
import time

from flask import Flask, render_template, request, jsonify

import gspread
from google.oauth2.service_account import Credentials

import socket

from luma.core.interface.serial import spi 
#This has a 3.5 inch TFT screen to display the IP address of the ethernet port and sensors as they connect
from luma.lcd.device import ili9488 
import pygame
from PIL import Image

import json
import os

## Sensor Calibration for adjusting sensors to a known good
# This is where your adjustments will be saved
CALIBRATION_FILE = "calibration.json"

def load_calibration():
    if os.path.exists(CALIBRATION_FILE):
        with open(CALIBRATION_FILE, "r") as f:
            return json.load(f)
    return {}

CALIBRATION = load_calibration()

# Config, let's set stuff up

# Data will be sent from the gateway ESP32 to the pi via the USB over, you guessed it, serial
SERIAL_PORT = "/dev/ttyUSB0"
BAUD_RATE = 115200

# Global Sensor Storage

sensor_data = {}
sheet_buffer = []
latest_for_sheet = {}

SENSOR_TIMEOUT = 300  # Send to google sheets every 5 minutes

# Serial Connection

ser = serial.Serial(
    SERIAL_PORT,
    BAUD_RATE,
    timeout=1
)

# TFT Setup
WIDTH = 480
HEIGHT = 320

pygame.init()

surface = pygame.Surface((WIDTH, HEIGHT))

serial = spi(
    port=0,
    device=0,
    gpio_DC=24,
    gpio_RST=25,
    bus_speed_hz=32000000
)

device = ili9488(serial, width=480, height=320)

font = pygame.font.SysFont("Arial", 24)
smallfont = pygame.font.SysFont("Arial", 18)


# Read In Serial Data

def serial_reader():

    global sensor_data

    buffer = ""

    while True:

        try:
            chunk = ser.read(ser.in_waiting or 1).decode(errors="ignore")
            buffer += chunk

            while "\n" in buffer:

                line, buffer = buffer.split("\n", 1)
                line = line.strip()

                if not line:
                    continue

                # must be JSON
                if not line.startswith("{") or not line.endswith("}"):
                    continue

                try:
                    data = json.loads(line)
                    sensor = data.get("sensor")

                    if sensor in CALIBRATION:

                        cal = CALIBRATION[sensor]

                        data["temperature"] += cal.get("temp", 0)
                        data["humidity"]    += cal.get("humidity", 0)
                        data["pressure"]    += cal.get("pressure", 0)
                except json.JSONDecodeError:
                    print("BAD JSON:", line)
                    continue

                sensor_name = data.get("sensor", "unknown")

                data["last_seen"] = int(time.time())

                sensor_data[sensor_name] = data

                # overwrite latest snapshot for sheets
                latest_for_sheet[sensor_name] = data

                print("STORED:", sensor_name)

        except Exception as e:
            print("SERIAL ERROR:", e)
            time.sleep(0.1)

# Google Sheets Shit
SCOPES = [
    "https://www.googleapis.com/auth/spreadsheets",
    "https://www.googleapis.com/auth/drive"
]

# The credentials.json is your own personal key you setup with google, you'll need to make your own
creds = Credentials.from_service_account_file(
    "credentials.json",
    scopes=SCOPES
)

client = gspread.authorize(creds)

sheet = client.open("ESP32 Weather Data").sheet1

#Google Sheets Function
def sheets_uploader():

    global latest_for_sheet

    while True:

        time.sleep(300)  # 5 minutes

        if not latest_for_sheet:
            continue

        try:
            print(f"Uploading {len(latest_for_sheet)} sensors to Sheets...")

            rows = []

            timestamp = time.strftime("%Y-%m-%d %H:%M:%S")

            for sensor, d in latest_for_sheet.items():

                rows.append([
                    timestamp,
                    sensor,
                    d.get("temperature"),
                    d.get("humidity"),
                    d.get("pressure"),
                    d.get("rssi")
                ])

            sheet.append_rows(rows)

            print("Upload complete")

        except Exception as e:
            print("Sheets upload error:", e)

# TFT Screen and IP shit.  This is crucial for when you deploy on site and you don't know what the IP your pi pulled will be
# This will be the IP that will display the web dashboard of sensor data, the IP to manage the pi, all of that.
def get_eth0_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "No IP"

def tft_loop():

    import time

    while True:

        surface.fill((0, 0, 0))

        # Title / IP
        ip = get_eth0_ip()
        title = font.render(f"ESP32 Weather Hub - {ip}", True, (255, 255, 255))
        surface.blit(title, (10, 5))

        # Sensor grid
        x_offset = 10
        y_offset = 40

        now = int(time.time())

        active_sensors = {
            k: v for k, v in sensor_data.items()
            if now - v.get("last_seen", 0) < SENSOR_TIMEOUT
        }

        max_sensors = list(active_sensors.items())[:8]

        for i, (name, d) in enumerate(max_sensors):

            col = i % 2
            row = i // 2

            x = x_offset + col * 230
            y = y_offset + row * 70

            try:
                temp = d.get("temperature", 0)
                hum = d.get("humidity", 0)

                age = now - d.get("last_seen", 0)
 
                # Nice little color-coded sensor check-in health stuff
                if age < 60:
                    color = (0, 255, 0)      # green
                elif age < 180:
                    color = (255, 165, 0)    # orange
                else:
                    color = (255, 0, 0)      # red

                box = smallfont.render(
                    f"{name}: {temp:.1f}F {hum:.1f}%",
                    True,
                    color
                )

                surface.blit(box, (x, y))

            except:
                pass

        # Push to TFT
        img_str = pygame.image.tostring(surface, "RGB")
        img = Image.frombytes("RGB", surface.get_size(), img_str)

        device.display(img)

        time.sleep(2)


# Flask App


app = Flask(__name__)


# Routes

@app.route("/")
def dashboard():
    try:
        return render_template("index.html")
    except Exception as e:
        return str(e), 500

@app.route("/api/data")
def api_data():

    now = int(time.time())
    cleaned = {}

    for sensor, d in sensor_data.items():

        age = now - d.get("last_seen", 0)

        d = dict(d)  # copy

        d["age"] = age

        if age < SENSOR_TIMEOUT:
            d["status"] = "online"
        else:
            d["status"] = "offline"

        cleaned[sensor] = d

    return jsonify(cleaned)

# Testing
@app.route("/test")
def test():
    return "Flask is working"

@app.route("/api/calibration", methods=["GET"])
def get_calibration():
    return jsonify(CALIBRATION)

@app.route("/api/calibration", methods=["POST"])
def save_calibration():

    global CALIBRATION

    CALIBRATION = request.json

    with open(CALIBRATION_FILE, "w") as f:
        json.dump(CALIBRATION, f, indent=2)

    return {"status": "ok"}

@app.route("/calibration")
def calibration_page():
    try:
        return render_template("calibration.html")
    except Exception as e:
        return str(e), 500
#@app.route("/calibration")
#def calibration_page():
#    return render_template("calibration.html")


# Main

if __name__ == "__main__":

    import threading

    t1 = threading.Thread(target=serial_reader, daemon=True)
    t2 = threading.Thread(target=sheets_uploader, daemon=True)
    t3 = threading.Thread(target=tft_loop, daemon=True)
    t1.start()
    t2.start()
    t3.start()

    app.run(host="0.0.0.0", port=5000, debug=False)
#if __name__ == "__main__":
