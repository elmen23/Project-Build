import React, { createContext, useContext, useState, useEffect, ReactNode } from "react";

interface Esp32ContextType {
  ip: string;
  setIp: (ip: string) => void;
  isOnline: boolean;
  setIsOnline: (status: boolean) => void;
  connectionError: string | null;
  setConnectionError: (error: string | null) => void;
}

const Esp32Context = createContext<Esp32ContextType | undefined>(undefined);

export function Esp32Provider({ children }: { children: ReactNode }) {
  const [ip, setIpState] = useState(() => {
    return localStorage.getItem("esp32-ip") || "192.168.4.1";
  });
  const [isOnline, setIsOnline] = useState(false);
  const [connectionError, setConnectionError] = useState<string | null>(null);

  const setIp = (newIp: string) => {
    setIpState(newIp);
    localStorage.setItem("esp32-ip", newIp);
    // Reset status when IP changes
    setIsOnline(false);
    setConnectionError(null);
  };

  return (
    <Esp32Context.Provider value={{ ip, setIp, isOnline, setIsOnline, connectionError, setConnectionError }}>
      {children}
    </Esp32Context.Provider>
  );
}

export function useEsp32Context() {
  const context = useContext(Esp32Context);
  if (!context) {
    throw new Error("useEsp32Context must be used within an Esp32Provider");
  }
  return context;
}
