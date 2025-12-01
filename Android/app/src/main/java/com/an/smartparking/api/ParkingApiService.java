package com.an.smartparking.api;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.an.smartparking.models.MasterDevice;
import com.an.smartparking.models.ParkingSpot;
import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;

public class ParkingApiService {
    private static final String TAG = "ParkingApiService";
    private static final String API_BASE_URL = "https://smart-parking-44.vercel.app/api";
    
    private final OkHttpClient client;
    private final Gson gson;
    private final Handler mainHandler;

    public ParkingApiService() {
        this.client = new OkHttpClient.Builder()
                .connectTimeout(15, TimeUnit.SECONDS)
                .readTimeout(15, TimeUnit.SECONDS)
                .writeTimeout(15, TimeUnit.SECONDS)
                .build();
        this.gson = new Gson();
        this.mainHandler = new Handler(Looper.getMainLooper());
    }

    public interface ParkingDataCallback {
        void onSuccess(Map<String, ParkingSpot> spots, MasterDevice master);
        void onError(String error);
    }

    public void fetchParkingData(final ParkingDataCallback callback) {
        Request request = new Request.Builder()
                .url(API_BASE_URL + "/parking")
                .get()
                .build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                Log.e(TAG, "Network request failed", e);
                mainHandler.post(() -> callback.onError("Network error: " + e.getMessage()));
            }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                if (!response.isSuccessful()) {
                    final String error = "Server error: " + response.code();
                    Log.e(TAG, error);
                    mainHandler.post(() -> callback.onError(error));
                    return;
                }

                try {
                    String responseBody = response.body().string();
                    Log.d(TAG, "Response: " + responseBody);

                    JsonObject jsonResponse = gson.fromJson(responseBody, JsonObject.class);
                    
                    if (!jsonResponse.has("success") || !jsonResponse.get("success").getAsBoolean()) {
                        String error = jsonResponse.has("error") ? 
                            jsonResponse.get("error").getAsString() : "Unknown error";
                        mainHandler.post(() -> callback.onError(error));
                        return;
                    }

                    JsonObject data = jsonResponse.getAsJsonObject("data");
                    
                    // Parse parking spots and master device
                    Map<String, ParkingSpot> parkingSpots = new HashMap<>();
                    MasterDevice masterDevice = null;

                    for (Map.Entry<String, JsonElement> entry : data.entrySet()) {
                        String key = entry.getKey();
                        JsonObject spotData = entry.getValue().getAsJsonObject();

                        if (key.equals("Devicemaster")) {
                            // Parse master device
                            masterDevice = new MasterDevice();
                            if (spotData.has("isOnline")) {
                                masterDevice.setOnline(spotData.get("isOnline").getAsBoolean());
                            }
                            if (spotData.has("System Status")) {
                                masterDevice.setSystemStatus(spotData.get("System Status").getAsString());
                            }
                            if (spotData.has("lastHealthPing")) {
                                masterDevice.setLastHealthPing(spotData.get("lastHealthPing").getAsString());
                            }
                        } else if (key.startsWith("Device") && !key.equals("DeviceCount")) {
                            // Parse parking spot
                            ParkingSpot spot = new ParkingSpot();
                            
                            if (spotData.has("Parking Status")) {
                                spot.setParkingStatus(spotData.get("Parking Status").getAsString());
                            }
                            if (spotData.has("System Status")) {
                                spot.setSystemStatus(spotData.get("System Status").getAsString());
                            }
                            if (spotData.has("Sensor Error")) {
                                spot.setSensorError(spotData.get("Sensor Error").getAsBoolean());
                            }
                            if (spotData.has("timeSinceUpdateMs")) {
                                spot.setTimeSinceUpdateMs(spotData.get("timeSinceUpdateMs").getAsLong());
                            }
                            if (spotData.has("removed")) {
                                spot.setRemoved(spotData.get("removed").getAsBoolean());
                            }

                            // Only add non-removed spots
                            if (!spot.isRemoved()) {
                                String deviceId = key.replace("Device", "");
                                parkingSpots.put(deviceId, spot);
                            }
                        }
                    }

                    final MasterDevice finalMaster = masterDevice;
                    final Map<String, ParkingSpot> finalSpots = parkingSpots;
                    
                    mainHandler.post(() -> callback.onSuccess(finalSpots, finalMaster));

                } catch (Exception e) {
                    Log.e(TAG, "Error parsing response", e);
                    mainHandler.post(() -> callback.onError("Error parsing data: " + e.getMessage()));
                }
            }
        });
    }

    public void cancelAllRequests() {
        client.dispatcher().cancelAll();
    }
}
