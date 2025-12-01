package com.an.smartparking.models;

public class ParkingSpot {
    private String parkingStatus;
    private String systemStatus;
    private boolean sensorError;
    private long timeSinceUpdateMs;
    private boolean removed;

    public ParkingSpot() {
    }

    public String getParkingStatus() {
        return parkingStatus;
    }

    public void setParkingStatus(String parkingStatus) {
        this.parkingStatus = parkingStatus;
    }

    public String getSystemStatus() {
        return systemStatus;
    }

    public void setSystemStatus(String systemStatus) {
        this.systemStatus = systemStatus;
    }

    public boolean isSensorError() {
        return sensorError;
    }

    public void setSensorError(boolean sensorError) {
        this.sensorError = sensorError;
    }

    public long getTimeSinceUpdateMs() {
        return timeSinceUpdateMs;
    }

    public void setTimeSinceUpdateMs(long timeSinceUpdateMs) {
        this.timeSinceUpdateMs = timeSinceUpdateMs;
    }

    public boolean isRemoved() {
        return removed;
    }

    public void setRemoved(boolean removed) {
        this.removed = removed;
    }

    public boolean isOccupied() {
        return "occupied".equalsIgnoreCase(parkingStatus);
    }

    public boolean isAvailable() {
        return "open".equalsIgnoreCase(parkingStatus);
    }

    public boolean isOnline() {
        return "online".equalsIgnoreCase(systemStatus);
    }
}
