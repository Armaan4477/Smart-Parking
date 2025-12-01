// Kill Switch Component to control Firebase communication
import { useKillSwitch } from '../context/KillSwitchContext';

export default function KillSwitch() {
  const { isKilled, isCommunicationEnabled } = useKillSwitch();

  return (
    <div className="bg-white rounded-lg shadow-md p-4 border-2" 
         style={{ borderColor: isKilled ? '#ef4444' : '#10b981' }}>
      <div className="flex items-center justify-between">
        <div className="flex-1">
          <h3 className="text-lg font-semibold mb-1">
            Firebase Communication Status
          </h3>
          <p className="text-sm text-gray-600">
            {isKilled 
              ? '🔴 All Firebase communication is DISABLED' 
              : '🟢 Firebase communication is ACTIVE'}
          </p>
        </div>
        
        <div
          className={`relative inline-flex h-10 w-20 items-center rounded-full ${
            isKilled 
              ? 'bg-red-500' 
              : 'bg-green-500'
          }`}
          role="status"
          aria-label={isKilled ? 'Disabled' : 'Active'}
        >
          <span
            className={`inline-block h-8 w-8 transform rounded-full bg-white shadow-lg transition-transform duration-300 ${
              isKilled ? 'translate-x-1' : 'translate-x-10'
            }`}
          />
        </div>
      </div>
      
      <div className="mt-3 text-xs text-gray-500">
        {isKilled 
          ? '⚠️ Hardcoded in code - Change KILL_SWITCH_ENABLED in KillSwitchContext.js to enable' 
          : 'ℹ️ Hardcoded in code - Change KILL_SWITCH_ENABLED in KillSwitchContext.js to disable'}
      </div>
    </div>
  );
}
