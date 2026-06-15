import { useState, useEffect, useCallback, useRef } from "react";
import { useEsp32Context } from "@/lib/esp32-context";

interface SystemStatus {
  running: boolean;
  softStart: boolean;
  freq: number;
  duty: number;
  dt: number;
  ss: number;
  wifi: {
    state: string;
    ip: string;
    rssi: number;
  };
}

export function useEsp32() {
  const { ip, setIsOnline, setConnectionError } = useEsp32Context();

  const fetchEsp32 = useCallback(async (path: string, options?: RequestInit) => {
    try {
      const url = `http://${ip}${path}`;
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 5000); // 5s timeout

      const response = await fetch(url, {
        ...options,
        signal: controller.signal,
        // mode: 'no-cors' might be needed if ESP32 doesn't send CORS headers, 
        // but we need to read JSON so we hope it sends CORS or we are on same network.
        // Assuming standard mode for now so we can parse JSON.
      });
      clearTimeout(timeoutId);

      setIsOnline(true);
      setConnectionError(null);

      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }

      // Handle empty responses
      const text = await response.text();
      return text ? JSON.parse(text) : {};
    } catch (err: any) {
      if (err.name === 'AbortError') {
        setConnectionError(`Timeout connecting to ESP32 at ${ip}`);
      } else {
        setConnectionError(`Cannot reach ESP32 at ${ip} — check connection`);
      }
      setIsOnline(false);
      throw err;
    }
  }, [ip, setIsOnline, setConnectionError]);

  return { fetchEsp32 };
}

export function useEsp32Status(intervalMs = 2500) {
  const { fetchEsp32 } = useEsp32();
  const [status, setStatus] = useState<SystemStatus | null>(null);
  const [loading, setLoading] = useState(true);

  const fetchStatus = useCallback(async () => {
    try {
      const data = await fetchEsp32('/status');
      setStatus(data);
    } catch (error) {
      // Error is handled by context
    } finally {
      setLoading(false);
    }
  }, [fetchEsp32]);

  useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, intervalMs);
    return () => clearInterval(interval);
  }, [fetchStatus, intervalMs]);

  return { status, loading, refetch: fetchStatus };
}
