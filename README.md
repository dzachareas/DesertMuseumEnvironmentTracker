The ESP32 boards in use are WROOM chips. I belive any ESP32 boards should work, just make sure
you select the appropriate board in the Ardino IDE.

The Raspberry Pi is a Pi 4, though any should work.

The gateway ESP32 sends it's data to the Pi through serial, so it needs to be connected to the pi
upon powering the pi as the service that runs the whole program will look for it and fail to start the
program.  Bonus feature, the ESP32 gateway will be powered by the pi, so there's one less power supply
that you'll need to provide.

The 3.5 inch TFT screen will display the ethernet address as well as any ESP32 readers, which will aid
greatly in troubleshooting.  The IP displayed on the screen will be the IP you can view the dashboard at,
as well as the calibration page for the sensors.  If you have time, you should power on all of the ESP32
readers and let the sensors run next to each other for a bit.  You can view the sensor readings on the
TFT screen, or in a web browser at the IP of the pi, on port 5000.  For example, it the IP shown on 
the TFT screen reads 192.168.1.12, then in a browser, on a machine on the same network, enter

http://192.168.1.12:5000 

Here you will see the dashboard of sensors.  After letting them run for a while, you can
equalize them by entering values into the claibration page at 

192.168.1.12:5000/calibration

Don't forget to click `save` to store your calibration adjustments.

Once you are happy with the setup and the readings, feel free to power down and install the readers
where they are intended to record data.

To generate your credentials file for sending data to your google sheet, go to

https://cdn-learn.adafruit.com/downloads/pdf/gdocs-sensor-logging-from-your-pc.pdf

And start on page 5, or view the whole document as it is a fun read and very interesting.  Plus, you can
see where I got some pieces for this project!  Adafruit is the best!


