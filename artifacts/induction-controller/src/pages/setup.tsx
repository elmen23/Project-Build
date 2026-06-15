import { useState, useEffect } from "react";
import { useEsp32 } from "@/hooks/use-esp32";
import { Card, CardContent, CardDescription, CardHeader, CardTitle, CardFooter } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Wifi, RefreshCw, Lock, CheckCircle2, AlertCircle } from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";

interface Network {
  ssid: string;
  rssi: number;
  secure: boolean;
}

export default function Setup() {
  const { fetchEsp32 } = useEsp32();
  
  const [scanning, setScanning] = useState(false);
  const [networks, setNetworks] = useState<Network[]>([]);
  const [selectedSsid, setSelectedSsid] = useState("");
  const [password, setPassword] = useState("");
  const [connecting, setConnecting] = useState(false);
  const [status, setStatus] = useState<{state: string; ip: string; rssi: number} | null>(null);
  const [error, setError] = useState<string | null>(null);

  const fetchStatus = async () => {
    try {
      const data = await fetchEsp32('/wifi-status');
      setStatus(data);
    } catch (e) {
      // Ignore poll errors
    }
  };

  useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, 3000);
    return () => clearInterval(interval);
  }, []);

  const handleScan = async () => {
    setScanning(true);
    setError(null);
    try {
      const result = await fetchEsp32('/scan');
      if (Array.isArray(result)) {
        setNetworks(result.sort((a, b) => b.rssi - a.rssi));
      } else if (result.scanning) {
        // ESP32 is currently scanning, wait and retry
        setTimeout(handleScan, 2000);
      }
    } catch (err: any) {
      setError("Failed to scan networks. Ensure ESP32 is reachable.");
    } finally {
      setScanning(false);
    }
  };

  const handleConnect = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!selectedSsid) return;

    setConnecting(true);
    setError(null);
    
    try {
      // Send as POST form data as typical in captive portals, or JSON depending on firmware
      // The prompt said POST with body ssid=&pass=
      await fetchEsp32('/connect', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: `ssid=${encodeURIComponent(selectedSsid)}&pass=${encodeURIComponent(password)}`
      });
      
      // Clear password field
      setPassword("");
      
      // Will be updated by poll
    } catch (err: any) {
      setError("Failed to send connect request.");
    } finally {
      setConnecting(false);
    }
  };

  const handleReset = async () => {
    try {
      await fetchEsp32('/reset-wifi', { method: 'POST' });
      fetchStatus();
    } catch (e) {
      setError("Failed to reset WiFi credentials.");
    }
  };

  return (
    <div className="max-w-2xl mx-auto space-y-6">
      <div>
        <h2 className="text-2xl font-bold font-mono tracking-tight">WIFI CONFIGURATION</h2>
        <p className="text-muted-foreground text-sm font-mono mt-1">Connect the induction heater to your local network</p>
      </div>

      <Card className="bg-card/50 border-border">
        <CardHeader>
          <CardTitle className="flex items-center gap-2 font-mono text-sm uppercase tracking-wider text-muted-foreground">
            <Wifi className="w-4 h-4 text-primary" />
            Current Status
          </CardTitle>
        </CardHeader>
        <CardContent>
          {status ? (
            <div className="flex items-center justify-between p-4 bg-background rounded-lg border border-border">
              <div className="flex items-center gap-4">
                <div className={`w-3 h-3 rounded-full ${status.state === 'connected' ? 'bg-green-500' : 'bg-yellow-500'}`} />
                <div>
                  <div className="font-mono font-bold uppercase">{status.state}</div>
                  {status.state === 'connected' && (
                    <div className="text-xs text-muted-foreground font-mono mt-1">IP: {status.ip} | RSSI: {status.rssi}dBm</div>
                  )}
                </div>
              </div>
              <Button variant="outline" size="sm" onClick={handleReset} className="font-mono text-xs">
                RESET CREDENTIALS
              </Button>
            </div>
          ) : (
            <div className="text-sm font-mono text-muted-foreground">Status unknown...</div>
          )}
        </CardContent>
      </Card>

      {error && (
        <Alert variant="destructive">
          <AlertCircle className="h-4 w-4" />
          <AlertTitle>Error</AlertTitle>
          <AlertDescription>{error}</AlertDescription>
        </Alert>
      )}

      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <Card className="bg-card/50 border-border">
          <CardHeader>
            <div className="flex items-center justify-between">
              <CardTitle className="font-mono text-sm uppercase tracking-wider text-muted-foreground">Available Networks</CardTitle>
              <Button variant="ghost" size="sm" onClick={handleScan} disabled={scanning} className="h-8 gap-2">
                <RefreshCw className={`w-3.5 h-3.5 ${scanning ? 'animate-spin' : ''}`} />
                <span className="font-mono text-xs">SCAN</span>
              </Button>
            </div>
          </CardHeader>
          <CardContent>
            {networks.length === 0 ? (
              <div className="text-center py-8 text-muted-foreground font-mono text-sm border border-dashed border-border rounded-lg">
                No networks found.<br/>Click Scan to search.
              </div>
            ) : (
              <div className="space-y-2 max-h-[300px] overflow-y-auto pr-2 custom-scrollbar">
                {networks.map((net, i) => (
                  <button
                    key={i}
                    className={`w-full flex items-center justify-between p-3 rounded-md border text-left transition-colors ${
                      selectedSsid === net.ssid 
                        ? 'bg-primary/10 border-primary text-foreground' 
                        : 'bg-background border-border hover:border-primary/50 text-muted-foreground'
                    }`}
                    onClick={() => setSelectedSsid(net.ssid)}
                  >
                    <div className="font-mono font-medium truncate pr-4">{net.ssid}</div>
                    <div className="flex items-center gap-3 shrink-0">
                      {net.secure && <Lock className="w-3.5 h-3.5 opacity-50" />}
                      <span className="text-xs font-mono w-12 text-right">{net.rssi}</span>
                    </div>
                  </button>
                ))}
              </div>
            )}
          </CardContent>
        </Card>

        <Card className="bg-card/50 border-border">
          <CardHeader>
            <CardTitle className="font-mono text-sm uppercase tracking-wider text-muted-foreground">Connect</CardTitle>
          </CardHeader>
          <CardContent>
            <form onSubmit={handleConnect} className="space-y-5">
              <div className="space-y-2">
                <Label htmlFor="ssid" className="font-mono text-xs uppercase text-muted-foreground">SSID</Label>
                <Input 
                  id="ssid" 
                  value={selectedSsid} 
                  onChange={(e) => setSelectedSsid(e.target.value)}
                  placeholder="Network name"
                  className="font-mono bg-background"
                  required
                />
              </div>
              
              <div className="space-y-2">
                <Label htmlFor="password" className="font-mono text-xs uppercase text-muted-foreground">Password</Label>
                <Input 
                  id="password" 
                  type="password"
                  value={password} 
                  onChange={(e) => setPassword(e.target.value)}
                  placeholder="Leave empty if open"
                  className="font-mono bg-background"
                />
              </div>

              <Button 
                type="submit" 
                className="w-full font-mono font-bold tracking-wider" 
                disabled={!selectedSsid || connecting}
              >
                {connecting ? "CONNECTING..." : "CONNECT"}
              </Button>
            </form>
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
