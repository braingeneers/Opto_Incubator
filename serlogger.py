# Noah Williams

"""
PROGRAM DESCRIPTION:
    Read and log data from serial port, created for visualizing PID results.

NOTES:
    Data format: temp,co2
    ls /dev/tty.*  to view all serial ports (Mac)

"""

# Libraries
import serial
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
import csv
import time

# Constants
FILENAME = time.strftime("%m%d%y_%H%M%S") + '.csv'
SERIAL_PORT = '/dev/tty.usbmodem101'
BAUD_RATE = 115200    # Match with Arduino
TIMEOUT = 1         # Timeout for serial port

TARGET_TEMP = 35
TARGET_CO2 = 5

# Variables
timePoints = []
tempValues = []
co2Values = []


# Open serial port
def openPort():
    try :
        open_port = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT) 
        print("Serial port is open")
        return open_port
    except serial.SerialException:
        print("Serial port is not open -- Exiting program")
        exit()


# Read data from serial port
def readData(port, csvOut=False,debug=True):
    data = port.readline().decode('utf-8').strip().split(',')
    if data and len(data) == 2:  # Ensure data is not empty and has two values
        try:
            timeVal = '%.2f' % (time.time() - startTime)
            timePoints.append(timeVal)
            tempValues.append(float(data[0]))  # Temperature value
            co2Values.append(float(data[1]))    # CO2 value

            # Write data to CSV file
            if csvOut:
                with open(FILENAME, mode='a') as file:
                    writer = csv.writer(file)
                    writer.writerow([timeVal, data[0], data[1]])


        except ValueError:
            print("Data conversion error -- Exiting program")
            exit()

# Plot two graphs at the same time, one for temperature and one for CO2
def update_plot(frame):

    # Read and append data from serial port
    readData(port)

    # Temp plot
    ax1.cla()
    ax1.set_ylim(15, 40)
    ax1.set_ylabel('Temperature (C)')
    ax1.set_xticks([])
    ax1.axhline(y=TARGET_TEMP, color='black', linestyle='--', label='Target Temp')
    ax1.plot(timePoints[-15:], tempValues[-15:], color='red')       # .5 hz, so 15 points is 30 seconds
    
    # CO2 plot
    ax2.cla()
    ax2.set_ylim(-1, 7)
    ax2.set_ylabel('CO2 (%)')
    ax2.set_xlabel('Time (30 Second Rolling Window)')
    ax2.set_xticks([])
    ax2.axhline(y=TARGET_CO2, color='black', linestyle='--', label='Target Temp')
    ax2.plot(timePoints[-15:], co2Values[-15:], color='blue')

     # Display current values in a text box
    if tempValues and co2Values:
        current_time = '%.2f' % (time.time() - startTime)
        current_temp = '%.2f' % tempValues[-1]
        current_co2 = '%.2f' % co2Values[-1]

        textstr = f'Time: {current_time} s\nTemp: {current_temp} C\nCO2: {current_co2} %'
        props = dict(boxstyle='round', facecolor='wheat', alpha=0.5)
        ax1.text(1.05, 0.5, textstr, transform=ax1.transAxes, fontsize=14, verticalalignment='center', bbox=props)


fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 6))
fig.suptitle('Temperature and CO2 vs Time')

port = openPort()
startTime = time.time()
plt.subplots_adjust(left=0.1, right=0.75, top=0.9, bottom=0.1, hspace=0.2)
ani = FuncAnimation(plt.gcf(), update_plot, interval=1000)
plt.show()
port.close()

# Create final graph with all data
plt.figure()
plt.plot(timePoints, tempValues, color='red')
plt.plot(timePoints, co2Values, color='blue')
plt.xlabel('Time (s)')
plt.xticks([])
plt.ylabel('Temperature (C) and CO2 (%)')
plt.title('Temperature and CO2 vs Time')
plt.legend(['Temperature', 'CO2'])
plt.show()


