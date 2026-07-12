## RUN AS ADMINISTRATOR

# 1. Enable the NTP Server feature in the Windows Time Service
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Services\W32Time\TimeProviders\NtpServer" -Name "Enabled" -Value 1

# 2. Force Windows to trust its own local clock (Crucial for isolated USB-Ethernet cables!)
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Services\W32Time\Config" -Name "AnnounceFlags" -Value 5

# 3. Restart the Time Service to apply the changes
Restart-Service w32time

# 4. Open UDP Port 123 in Windows Firewall so Jerry's packets are accepted
New-NetFirewallRule -Name "Allow Inbound NTP" -DisplayName "Allow Inbound NTP (Jerry Firmware)" -Direction Inbound -Protocol UDP -LocalPort 123 -Action Allow