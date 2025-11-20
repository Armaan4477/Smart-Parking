package com.an.smartparking.models;

import java.util.Map;

public class ParkingData {
    private boolean success;
    private Map<String, Object> data;

    public ParkingData() {
    }

    public boolean isSuccess() {
        return success;
    }

    public void setSuccess(boolean success) {
        this.success = success;
    }

    public Map<String, Object> getData() {
        return data;
    }

    public void setData(Map<String, Object> data) {
        this.data = data;
    }
}
