
extern u8 sleep_states[];
extern int acpi_suspend (u32 state);

extern void acpi_enable_wakeup_device_prep(u8 sleep_state);
extern void acpi_enable_wakeup_device(u8 sleep_state);
extern void acpi_disable_wakeup_device(u8 sleep_state);
extern void acpi_wakeup_gpe_poweroff_prepare(void);
