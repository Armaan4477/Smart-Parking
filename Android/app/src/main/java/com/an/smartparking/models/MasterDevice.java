package com.an.smartparking.models;

public class MasterDevice {
    private boolean isOnline;
    private String systemStatus;
    private String lastHealthPing;

    public MasterDevice() {
    }

    public boolean isOnline() {
        return isOnline;
    }

    public void setOnline(boolean online) {
        isOnline = online;
    }

    public String getSystemStatus() {
        return systemStatus;
    }

    public void setSystemStatus(String systemStatus) {
        this.systemStatus = systemStatus;
    }

    public String getLastHealthPing() {
        return lastHealthPing;
    }

    public void setLastHealthPing(String lastHealthPing) {
        this.lastHealthPing = lastHealthPing;
    }
}
