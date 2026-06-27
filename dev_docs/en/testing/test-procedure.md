# Test Procedure - Basic Modes

## Preparation
1. Connect [**Serial Monitor**](https://serial-studio.com/). Open the handler file `enterprise_pu.ssproj`.
2. Install and configure **PuTTY** for connecting to the controller's COM port.

---

## Heating Test
- Start drying and ensure that the air temperature consistently reaches **60 °C**.  
- Increase the temperature in **10 °C steps**, up to **100 °C**, monitoring the rise and stabilization.  

---

## Automatic PID Tuning

1. Connect to the controller via **PuTTY**.  
2. Perform **PID autotune** for the heater  
   → *Settings → PID HEATER → Autopid*  
   - After completion, copy the values of **Kp, Ki, Kd, Gain** from the terminal / PuTTY.  
3. Perform **PID autotune** for the chamber  
   → *Settings → PID CHAMBER → Autopid*  
   - Record the obtained coefficients **Kp, Ki, Kd, Gain** as well.  

---

## Storage Mode Test by Temperature

**Settings:**
- Storage priority: **by temperature**.  
- Parameters:  
  - Drying - **60 °C**, time **10 minutes**.  
  - Storage - **45 °C**.  

**Expected Result:**
- After drying is complete, the device automatically switches to **Storage** mode.  
- The air temperature is maintained around **45 °C**, with the heater switching on and off within the hysteresis range.  

---

## Storage Mode Test by Humidity

**Settings:**
- Storage priority: **by humidity**.  
- Target humidity - approximately **10% lower than the current actual value** (for faster verification).  
- Enable drying at **60 °C** for **3 minutes**.  

**Expected Result:**
- After drying is complete, the device switches to **Storage** mode.  
- Once the target humidity is reached, the heater and fan automatically turn off.  
- Verify proper automatic **on/off operation** of the heater and fan as humidity changes.  

---

## Additional Verification

- Check the transition to storage mode:
  - by **absolute temperature**;
  - by **temperature defined as a percentage of drying temperature**.

---

**Note:**  
During testing, record all observations in the logs (Serial / PuTTY) and mark device behavior at each step.
