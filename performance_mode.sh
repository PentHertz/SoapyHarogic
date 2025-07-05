#!/usr/bin/env bash

#
# SDR Performance Mode Script
#
# This script applies several system tweaks to optimize a Linux system for
# high-throughput, low-latency tasks, such as running a high-speed SDR.
#
# USAGE:
#   Run this script with superuser privileges before starting your SDR application.
#   $ sudo ./sdr_performance_mode.sh
#

# Exit on any error, treat unset variables as errors, and prevent errors in pipelines from being masked.
set -o errexit
set -o nounset
set -o pipefail

# --- Configuration ---
# The CPU core to which the USB IRQ will be pinned.
# Core 1 is often a good choice to avoid contention with Core 0.
IRQ_TARGET_CPU=1

# --- Style and Formatting ---
C_RED='\033[0;31m'
C_GREEN='\033[0;32m'
C_YELLOW='\033[0;33m'
C_BLUE='\033[0;34m'
C_NC='\033[0m' # No Color

# --- Prerequisite Check ---
if [[ "$EUID" -ne 0 ]]; then
  echo -e "${C_RED}Error: This script must be run with sudo or as root.${C_NC}"
  echo "Example: sudo $0"
  exit 1
fi

# ==============================================================================
# OPTIMIZATION FUNCTIONS
# ==============================================================================

#
# Set the CPU scaling governor to 'performance'.
# This prevents the CPU from down-clocking, reducing latency.
# See: https://docs.kernel.org/admin-guide/pm/cpufreq.html
#
set_performance_governor() {
  echo -e "\n${C_BLUE}--- Setting CPU Governor to 'performance' ---${C_NC}"
  for cpu_gov in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo "performance" > "${cpu_gov}"
  done
  echo -e "${C_GREEN}Success:${C_NC} All CPU cores now set to 'performance' governor."
}

#
# Disable USB core autosuspend.
# This prevents the kernel from trying to power-save USB devices that are actively streaming.
#
set_usb_autosuspend() {
  echo -e "\n${C_BLUE}--- Disabling USB Core Autosuspend ---${C_NC}"
  if [[ -f /sys/module/usbcore/parameters/autosuspend ]]; then
    echo -1 > /sys/module/usbcore/parameters/autosuspend
    echo -e "${C_GREEN}Success:${C_NC} USB autosuspend has been disabled."
  else
    echo -e "${C_YELLOW}Warning:${C_NC} Could not find usbcore autosuspend parameter file. Skipping."
  fi
}

#
# Increase the memory buffer for the USB file system (usbfs).
# This provides more memory for handling asynchronous USB requests, vital for high sample rates.
#
set_usbfs_memory() {
  echo -e "\n${C_BLUE}--- Increasing USB-FS Memory Buffer ---${C_NC}"
  if [[ -f /sys/module/usbcore/parameters/usbfs_memory_mb ]]; then
    # Set to a generous 1000 MB. The default is often a mere 16 MB.
    echo 1000 > /sys/module/usbcore/parameters/usbfs_memory_mb
    echo -e "${C_GREEN}Success:${C_NC} usbfs memory limit increased to 1000 MB."
  else
    echo -e "${C_YELLOW}Warning:${C_NC} Could not find usbfs_memory_mb parameter file. Skipping."
  fi
}

#
# Set IRQ affinity for the USB 3.0 (xHCI) controller.
# Pinning the USB controller's interrupt to a specific CPU core can significantly reduce latency.
#
set_irq_affinity() {
  echo -e "\n${C_BLUE}--- Setting USB 3.0 (xHCI) IRQ Affinity ---${C_NC}"
  # Find the IRQ number for the xhci_hcd (USB 3.0 controller)
  local xhci_irq
  xhci_irq=$(grep 'xhci_hcd' /proc/interrupts | head -n 1 | awk '{print $1}' | tr -d ':')

  if [[ -z "$xhci_irq" ]]; then
    echo -e "${C_YELLOW}Warning:${C_NC} Could not find an active IRQ for xhci_hcd. Skipping."
    return
  fi

  # Calculate the CPU mask for the target core.
  # e.g., for CPU 1, mask is 2 (binary 10); for CPU 2, mask is 4 (binary 100)
  local cpu_mask=$((1 << IRQ_TARGET_CPU))

  echo "Found xHCI controller at IRQ ${xhci_irq}. Pinning to CPU ${IRQ_TARGET_CPU} (mask: ${cpu_mask})."
  echo "${cpu_mask}" > "/proc/irq/${xhci_irq}/smp_affinity"
  echo -e "${C_GREEN}Success:${C_NC} USB 3.0 IRQ affinity has been set."
}

#
# DRM KMS polling can sometimes be expensive. Disabling it can sometimes help performance.
# See: https://wiki.archlinux.org/title/kernel_mode_setting#Problem_upon_bootloading_and_dmesg
#
set_kms_polling() {
  echo -e "\n${C_BLUE}--- Disabling DRM KMS Polling ---${C_NC}"
  if [[ -f /sys/module/drm_kms_helper/parameters/poll ]]; then
    echo "N" > /sys/module/drm_kms_helper/parameters/poll
    echo -e "${C_GREEN}Success:${C_NC} Disabled DRM KMS polling."
  else
    echo -e "${C_YELLOW}Warning:${C_NC} Could not find drm_kms_helper poll parameter file. Skipping."
  fi
}

#
# Function to apply all optimizations non-interactively.
#
apply_all_optimizations() {
    set_performance_governor
    set_usb_autosuspend
    set_usbfs_memory
    set_irq_affinity
    set_kms_polling
    echo -e "\n${C_GREEN}All performance optimizations have been applied.${C_NC}"
}

#
# Reverts settings to more standard, power-saving defaults.
#
revert_all_settings() {
  echo -e "\n${C_BLUE}--- Reverting Settings to Defaults ---${C_NC}"

  # Revert CPU governor
  local default_governor="powersave"
  echo "Setting CPU governor to '${default_governor}'..."
  for cpu_gov in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo "${default_governor}" > "${cpu_gov}"
  done

  # Revert USB autosuspend
  if [[ -f /sys/module/usbcore/parameters/autosuspend ]]; then
    echo "Enabling USB autosuspend (2 seconds)..."
    echo 2 > /sys/module/usbcore/parameters/autosuspend
  fi
  
  # Revert KMS Polling
  if [[ -f /sys/module/drm_kms_helper/parameters/poll ]]; then
    echo "Enabling DRM KMS polling..."
    echo "Y" > /sys/module/drm_kms_helper/parameters/poll
  fi
  
  echo -e "\n${C_YELLOW}Note:${C_NC} IRQ affinity and USB-FS memory buffers are best reset by a system reboot."
  echo -e "${C_GREEN}Revert process complete. A reboot is recommended to restore all original defaults.${C_NC}"
}


# ==============================================================================
# MAIN SCRIPT LOGIC
# ==============================================================================

show_menu() {
    echo -e "\n${C_BLUE}=========================================${C_NC}"
    echo -e "   ${C_YELLOW}SDR Performance Optimization Tool${C_NC}"
    echo -e "${C_BLUE}=========================================${C_NC}"
    echo "Select an option:"
    echo "  1) Set CPU Governor to 'performance'"
    echo "  2) Disable USB Autosuspend"
    echo "  3) Increase USB-FS Memory Buffer"
    echo "  4) Set USB 3.0 IRQ Affinity"
    echo "  5) Disable DRM KMS Polling"
    echo "  -------------------------------------"
    echo "  ${C_GREEN}a) Apply ALL Performance Tweaks${C_NC}"
    echo "  ${C_YELLOW}r) Revert to Default Settings${C_NC}"
    echo "  ${C_RED}q) Quit${C_NC}"
    echo -e "${C_BLUE}=========================================${C_NC}"
}

main() {
    while true; do
        show_menu
        read -r -p "Enter your choice: " choice
        case "$choice" in
            1) set_performance_governor ;;
            2) set_usb_autosuspend ;;
            3) set_usbfs_memory ;;
            4) set_irq_affinity ;;
            5) set_kms_polling ;;
            a|A) apply_all_optimizations ;;
            r|R) revert_all_settings ;;
            q|Q) break ;;
            *) echo -e "${C_RED}Invalid option. Please try again.${C_NC}" ;;
        esac
    done
    echo -e "\nExiting script."
}

# Run the main function
main
