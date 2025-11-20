package com.an.smartparking;

import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.cardview.widget.CardView;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.an.smartparking.api.ParkingApiService;
import com.an.smartparking.models.MasterDevice;
import com.an.smartparking.models.ParkingSpot;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.Map;

public class MainActivity extends AppCompatActivity {
    
    private ParkingApiService apiService;
    private Handler autoRefreshHandler;
    private Runnable autoRefreshRunnable;
    
    // UI Components
    private SwipeRefreshLayout swipeRefresh;
    private View statusIndicator;
    private TextView parkingStatusText;
    private TextView masterStatusText;
    private TextView totalSpotsValue;
    private TextView availableSpotsValue;
    private TextView occupiedSpotsValue;
    private TextView lastUpdatedText;
    private CardView errorCard;
    private TextView errorText;
    private Button retryButton;
    private CardView masterStatusCard;
    
    // Auto-refresh interval (5 seconds)
    private static final long AUTO_REFRESH_INTERVAL = 5000;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        // Initialize API service
        apiService = new ParkingApiService();
        autoRefreshHandler = new Handler(Looper.getMainLooper());
        
        // Initialize views
        initViews();
        
        // Set up refresh listener
        swipeRefresh.setOnRefreshListener(() -> fetchParkingData(true));
        
        // Set up retry button
        retryButton.setOnClickListener(v -> fetchParkingData(true));
        
        // Initial data fetch
        fetchParkingData(true);
        
        // Start auto-refresh
        startAutoRefresh();
    }
    
    private void initViews() {
        swipeRefresh = findViewById(R.id.swipeRefresh);
        statusIndicator = findViewById(R.id.statusIndicator);
        parkingStatusText = findViewById(R.id.parkingStatusText);
        masterStatusText = findViewById(R.id.masterStatusText);
        totalSpotsValue = findViewById(R.id.totalSpotsValue);
        availableSpotsValue = findViewById(R.id.availableSpotsValue);
        occupiedSpotsValue = findViewById(R.id.occupiedSpotsValue);
        lastUpdatedText = findViewById(R.id.lastUpdatedText);
        errorCard = findViewById(R.id.errorCard);
        errorText = findViewById(R.id.errorText);
        retryButton = findViewById(R.id.retryButton);
        masterStatusCard = findViewById(R.id.masterStatusCard);
    }
    
    private void fetchParkingData(boolean showLoading) {
        if (showLoading) {
            swipeRefresh.setRefreshing(true);
            errorCard.setVisibility(View.GONE);
        }
        
        apiService.fetchParkingData(new ParkingApiService.ParkingDataCallback() {
            @Override
            public void onSuccess(Map<String, ParkingSpot> spots, MasterDevice master) {
                if (showLoading) {
                    swipeRefresh.setRefreshing(false);
                }
                updateUI(spots, master);
            }

            @Override
            public void onError(String error) {
                if (showLoading) {
                    swipeRefresh.setRefreshing(false);
                }
                showError(error);
            }
        });
    }
    
    private void updateUI(Map<String, ParkingSpot> spots, MasterDevice master) {
        // Calculate statistics
        int totalSpots = spots.size();
        int availableSpots = 0;
        int occupiedSpots = 0;
        
        for (ParkingSpot spot : spots.values()) {
            if (spot.isOnline() && !spot.isSensorError()) {
                if (spot.isAvailable()) {
                    availableSpots++;
                } else if (spot.isOccupied()) {
                    occupiedSpots++;
                }
            }
        }
        
        // Update spot counts
        totalSpotsValue.setText(String.valueOf(totalSpots));
        availableSpotsValue.setText(String.valueOf(availableSpots));
        occupiedSpotsValue.setText(String.valueOf(occupiedSpots));
        
        // Update master status and parking status
        boolean isParkingOpen = master != null && master.isOnline();
        
        if (isParkingOpen) {
            parkingStatusText.setText("Parking: OPEN");
            parkingStatusText.setTextColor(Color.parseColor("#388E3C"));
            masterStatusText.setText("Master ESP32: Online");
            
            // Update status indicator to green
            GradientDrawable drawable = (GradientDrawable) statusIndicator.getBackground();
            drawable.setColor(Color.parseColor("#4CAF50"));
            
            masterStatusCard.setCardBackgroundColor(Color.parseColor("#E8F5E9"));
        } else {
            parkingStatusText.setText("Parking: CLOSED");
            parkingStatusText.setTextColor(Color.parseColor("#D32F2F"));
            masterStatusText.setText("Master ESP32: Offline");
            
            // Update status indicator to red
            GradientDrawable drawable = (GradientDrawable) statusIndicator.getBackground();
            drawable.setColor(Color.parseColor("#F44336"));
            
            masterStatusCard.setCardBackgroundColor(Color.parseColor("#FFEBEE"));
        }
        
        // Update last updated time
        SimpleDateFormat sdf = new SimpleDateFormat("MMM dd, yyyy HH:mm:ss", Locale.getDefault());
        lastUpdatedText.setText("Last updated: " + sdf.format(new Date()));
    }
    
    private void showError(String error) {
        errorCard.setVisibility(View.VISIBLE);
        errorText.setText("Error: " + error);
        
        // Set default values
        totalSpotsValue.setText("--");
        availableSpotsValue.setText("--");
        occupiedSpotsValue.setText("--");
        parkingStatusText.setText("Parking: UNKNOWN");
        parkingStatusText.setTextColor(Color.parseColor("#FF6F00"));
        masterStatusText.setText("Unable to connect to server");
        
        // Update status indicator to orange/yellow
        GradientDrawable drawable = (GradientDrawable) statusIndicator.getBackground();
        drawable.setColor(Color.parseColor("#FF9800"));
        
        masterStatusCard.setCardBackgroundColor(Color.parseColor("#FFF3E0"));
    }
    
    private void startAutoRefresh() {
        autoRefreshRunnable = new Runnable() {
            @Override
            public void run() {
                fetchParkingData(false); // Background refresh without showing loading spinner
                autoRefreshHandler.postDelayed(this, AUTO_REFRESH_INTERVAL);
            }
        };
        autoRefreshHandler.postDelayed(autoRefreshRunnable, AUTO_REFRESH_INTERVAL);
    }
    
    private void stopAutoRefresh() {
        if (autoRefreshHandler != null && autoRefreshRunnable != null) {
            autoRefreshHandler.removeCallbacks(autoRefreshRunnable);
        }
    }
    
    @Override
    protected void onResume() {
        super.onResume();
        fetchParkingData(true);
        startAutoRefresh();
    }
    
    @Override
    protected void onPause() {
        super.onPause();
        stopAutoRefresh();
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopAutoRefresh();
        if (apiService != null) {
            apiService.cancelAllRequests();
        }
    }
}