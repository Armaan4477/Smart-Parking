// Kill Switch Context to control Firebase communication
import { createContext, useContext } from 'react';

const KillSwitchContext = createContext();

export const useKillSwitch = () => {
  const context = useContext(KillSwitchContext);
  if (!context) {
    throw new Error('useKillSwitch must be used within a KillSwitchProvider');
  }
  return context;
};

// ⚠️ HARDCODED KILL SWITCH VALUE - Change this value to control Firebase communication
// Set to true to DISABLE all Firebase communication
// Set to false to ENABLE all Firebase communication
export const KILL_SWITCH_ENABLED = false;

export const KillSwitchProvider = ({ children }) => {
  // Use the hardcoded value
  const isKilled = KILL_SWITCH_ENABLED;

  // These functions are no-ops since the value is hardcoded
  // They are kept for API compatibility
  const toggleKillSwitch = () => {
    console.warn('Kill switch is hardcoded. Change KILL_SWITCH_ENABLED in KillSwitchContext.js to toggle.');
  };

  const enableCommunication = () => {
    console.warn('Kill switch is hardcoded. Change KILL_SWITCH_ENABLED in KillSwitchContext.js to enable.');
  };

  const disableCommunication = () => {
    console.warn('Kill switch is hardcoded. Change KILL_SWITCH_ENABLED in KillSwitchContext.js to disable.');
  };

  return (
    <KillSwitchContext.Provider 
      value={{ 
        isKilled, 
        toggleKillSwitch, 
        enableCommunication, 
        disableCommunication,
        isCommunicationEnabled: !isKilled
      }}
    >
      {children}
    </KillSwitchContext.Provider>
  );
};
