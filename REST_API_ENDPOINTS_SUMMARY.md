# REST API Endpoints Added to AstroPixelsPlus

## Summary

Successfully added 4 new REST API endpoints to `AsyncWebInterface.h` for controlling smoke, fire effects, CBI (Charge Bay Indicator), and DataPanel features of the R2-D2 dome.

## New Endpoints

### 1. Smoke Control
- **POST /api/smoke** - Control smoke effects
  - Parameters: `state` ("on" or "off")
  - Commands: `BMON` (on), `BMOFF` (off)
  - Example: `curl -X POST http://192.168.4.1/api/smoke -d 'state=on'`

### 2. Fire Effects Control  
- **POST /api/fire** - Control fire strip effects
  - Parameters: `state` ("on" or "off")
  - Commands: `FS11000` (on for 1000ms), `FSOFF` (off)
  - Example: `curl -X POST http://192.168.4.1/api/fire -d 'state=on'`

### 3. CBI (Charge Bay Indicator) Control
- **GET /api/cbi** - Get current CBI state (returns "unknown" - no direct state tracking)
- **POST /api/cbi** - Control CBI effects
  - Parameters: `action` ("flicker" or "disable"), optional `duration` (seconds, defaults: 6s flicker, 8s disable)
  - Commands: `CB2{duration}006` (flicker), `CB1{duration}008` (disable)
  - Examples:
    - `curl -X POST http://192.168.4.1/api/cbi -d 'action=flicker'`
    - `curl -X POST http://192.168.4.1/api/cbi -d 'action=flicker&duration=5'`

### 4. DataPanel Control
- **GET /api/datapanel** - Get current DataPanel state (returns "unknown" - no direct state tracking)
- **POST /api/datapanel** - Control DataPanel effects
  - Parameters: `action` ("flicker" or "disable"), optional `duration` (seconds, defaults: 6s flicker, 8s disable)
  - Commands: `DP2{duration}006` (flicker), `DP1{duration}008` (disable)
  - Examples:
    - `curl -X POST http://192.168.4.1/api/datapanel -d 'action=flicker'`
    - `curl -X POST http://192.168.4.1/api/datapanel -d 'action=disable&duration=3'`

## Implementation Details

### Code Location
- File: `AsyncWebInterface.h`
- Function: `initAsyncWeb()` (lines 812-962)
- Added after existing `/api/wake` endpoint, before `/upload/firmware` endpoint

### Design Patterns Followed
1. **Authentication**: All POST endpoints use `checkWriteAuth(request)` for security
2. **Error Handling**: Proper HTTP status codes and JSON error responses
3. **Parameter Validation**: Validates required parameters and acceptable values
4. **Command Processing**: Uses `processMarcduinoCommandWithSource()` with "astropixel-web-api" source
5. **JSON Responses**: Consistent JSON response format matching existing endpoints
6. **Sleep Mode Check**: Commands are automatically blocked if system is in sleep mode via `shouldBlockCommandDuringSleep()`

### Response Formats
- **Success**: `{"ok":true,"state":"on"}` or `{"ok":true,"action":"flicker","duration":"5"}`
- **Error**: `{"error":"unauthorized"}`, `{"error":"invalid state, use 'on' or 'off'"}`, etc.
- **Status Codes**: 200 (success), 401 (unauthorized), 400 (bad request)

### Command Mapping
Based on existing sequence definitions in `MarcduinoSequence.h`:
- **Smoke**: `BMON` / `BMOFF` (from line 82, 87 in SE06 sequence)
- **Fire**: `FS11000` / `FSOFF` (from line 76, 89 in SE06 sequence)  
- **CBI**: `CB2{duration}006` / `CB1{duration}008` (from line 78, 94 in SE06 sequence)
- **DataPanel**: `DP2{duration}006` / `DP1{duration}008` (from line 80, 96 in SE06 sequence)

## Testing

### Build Verification
- ✅ Code compiles successfully with PlatformIO
- ✅ No syntax errors or compilation issues
- ✅ Follows existing code patterns and conventions

### Test Script
Created `test_new_endpoints.py` for comprehensive testing of all new endpoints including:
- Valid command testing
- Error case testing  
- Usage examples with curl commands
- Parameter validation testing

### Manual Testing Commands
```bash
# Smoke control
curl -X POST http://192.168.4.1/api/smoke -d 'state=on'
curl -X POST http://192.168.4.1/api/smoke -d 'state=off'

# Fire effects control
curl -X POST http://192.168.4.1/api/fire -d 'state=on' 
curl -X POST http://192.168.4.1/api/fire -d 'state=off'

# CBI control
curl -X GET http://192.168.4.1/api/cbi
curl -X POST http://192.168.4.1/api/cbi -d 'action=flicker&duration=5'
curl -X POST http://192.168.4.1/api/cbi -d 'action=disable&duration=8'

# DataPanel control
curl -X GET http://192.168.4.1/api/datapanel
curl -X POST http://192.168.4.1/api/datapanel -d 'action=flicker&duration=4'
curl -X POST http://192.168.4.1/api/datapanel -d 'action=disable&duration=2'
```

## Integration Notes

### Authentication
- Uses existing `checkWriteAuth()` function
- Supports API token via `X-AP-Token` header or `token` POST parameter
- If no token configured, endpoints are open (logs warning)

### Command Source Tracking
- All commands logged with source "astropixel-web-api" 
- Appears in logs as: `[CMD][astropixel-web-api] BMON`

### Sleep Mode Integration
- Commands automatically blocked if system is in sleep mode
- Returns HTTP 423 with hint to use `/api/wake` endpoint

### WebSocket Broadcasting
- State changes broadcast to WebSocket clients via existing `broadcastState()` mechanism

## Files Modified
- `AsyncWebInterface.h` - Added 4 new REST API endpoints (150 lines added)
- `test_new_endpoints.py` - Created comprehensive test script (162 lines)

## Verification
- ✅ Code compiles without errors
- ✅ Follows existing endpoint patterns exactly
- ✅ Proper authentication and error handling
- ✅ Consistent JSON response format
- ✅ Integration with existing Marcduino command system
- ✅ Sleep mode and WebSocket integration maintained