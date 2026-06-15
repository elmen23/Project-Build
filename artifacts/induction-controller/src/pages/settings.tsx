import { useState } from "react";
import { useEsp32Context } from "@/lib/esp32-context";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Settings2, Save } from "lucide-react";
import { toast } from "sonner";

export default function Settings() {
  const { ip, setIp } = useEsp32Context();
  const [localIp, setLocalIp] = useState(ip);

  const handleSave = (e: React.FormEvent) => {
    e.preventDefault();
    if (localIp.trim()) {
      setIp(localIp.trim());
      toast.success("ESP32 IP address updated");
    }
  };

  return (
    <div className="max-w-xl mx-auto space-y-6">
      <div>
        <h2 className="text-2xl font-bold font-mono tracking-tight">SETTINGS</h2>
        <p className="text-muted-foreground text-sm font-mono mt-1">Configure dashboard connection parameters</p>
      </div>

      <Card className="bg-card/50 border-border">
        <CardHeader>
          <CardTitle className="flex items-center gap-2 font-mono text-sm uppercase tracking-wider text-muted-foreground">
            <Settings2 className="w-4 h-4 text-primary" />
            Connection Settings
          </CardTitle>
        </CardHeader>
        <CardContent>
          <form onSubmit={handleSave} className="space-y-6">
            <div className="space-y-3">
              <Label htmlFor="ip" className="font-mono text-xs uppercase text-muted-foreground">ESP32 IP Address / Hostname</Label>
              <div className="flex gap-3">
                <Input 
                  id="ip" 
                  value={localIp} 
                  onChange={(e) => setLocalIp(e.target.value)}
                  placeholder="e.g., 192.168.4.1"
                  className="font-mono bg-background flex-1"
                />
                <Button type="submit" className="gap-2 font-mono">
                  <Save className="w-4 h-4" />
                  SAVE
                </Button>
              </div>
              <p className="text-xs font-mono text-muted-foreground opacity-70">
                Default AP IP is 192.168.4.1. If connected to local network, enter the allocated IP.
              </p>
            </div>
          </form>
        </CardContent>
      </Card>
    </div>
  );
}
