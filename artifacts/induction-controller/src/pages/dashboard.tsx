import { useState, useEffect, useCallback, useRef } from "react";
import { useEsp32, useEsp32Status } from "@/hooks/use-esp32";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Slider } from "@/components/ui/slider";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Power, Activity, Zap, Clock, Timer, Gauge } from "lucide-react";

interface ControlParams {
  f: number;
  d: number;
  dt: number;
  ss: number;
}

const DEFAULT_PARAMS: ControlParams = {
  f: 100000,
  d: 45,
  dt: 500,
  ss: 2000,
};

export default function Dashboard() {
  const { status, loading, refetch } = useEsp32Status(2500);
  const { fetchEsp32 } = useEsp32();
  
  const [params, setParams] = useState<ControlParams>(() => {
    try {
      const saved = localStorage.getItem("esp32-params");
      if (saved) return JSON.parse(saved);
    } catch (e) {}
    return DEFAULT_PARAMS;
  });

  const [applying, setApplying] = useState(false);
  const [pendingAction, setPendingAction] = useState<"start" | "stop" | null>(null);

  // Debounce saving parameters
  const saveTimeout = useRef<NodeJS.Timeout | null>(null);
  
  const updateParam = (key: keyof ControlParams, value: number) => {
    const newParams = { ...params, [key]: value };
    setParams(newParams);
    
    // Save to local storage
    localStorage.setItem("esp32-params", JSON.stringify(newParams));
    
    // Auto-apply parameters (debounced)
    if (saveTimeout.current) clearTimeout(saveTimeout.current);
    saveTimeout.current = setTimeout(async () => {
      setApplying(true);
      try {
        await fetchEsp32(`/set?f=${newParams.f}&d=${newParams.d}&dt=${newParams.dt}&ss=${newParams.ss}`);
        refetch();
      } catch (e) {
        // Handled in hook
      } finally {
        setApplying(false);
      }
    }, 500);
  };

  const handleStart = async () => {
    setPendingAction("start");
    try {
      await fetchEsp32('/start');
      refetch();
    } catch (e) {
    } finally {
      setPendingAction(null);
    }
  };

  const handleStop = async () => {
    setPendingAction("stop");
    try {
      await fetchEsp32('/stop');
      refetch();
    } catch (e) {
    } finally {
      setPendingAction(null);
    }
  };

  const isRunning = status?.running;
  const isSoftStarting = status?.softStart;
  
  let statusColor = "bg-red-500";
  let statusText = "STOPPED";
  
  if (isRunning) {
    if (isSoftStarting) {
      statusColor = "bg-yellow-500 animate-pulse-slow";
      statusText = "STARTING";
    } else {
      statusColor = "bg-green-500 animate-pulse-slow shadow-[0_0_15px_rgba(34,197,94,0.5)]";
      statusText = "RUNNING";
    }
  } else if (!status && loading) {
    statusColor = "bg-gray-500";
    statusText = "CONNECTING";
  }

  return (
    <div className="space-y-6">
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        {/* Status Panel */}
        <Card className="col-span-1 md:col-span-3 border-border bg-card/80 backdrop-blur">
          <CardContent className="p-6 md:p-8 flex flex-col md:flex-row items-center justify-between gap-6">
            <div className="flex items-center gap-6">
              <div className="relative flex items-center justify-center">
                <div className={`w-16 h-16 rounded-full flex items-center justify-center border-2 border-card ${statusColor.replace('bg-', 'text-').split(' ')[0]}`}>
                  <Power className={`w-8 h-8 ${statusColor.replace('bg-', 'text-').split(' ')[0]}`} />
                </div>
                <div className={`absolute inset-0 rounded-full opacity-20 ${statusColor}`} />
              </div>
              <div>
                <div className="text-sm font-mono text-muted-foreground uppercase tracking-widest mb-1">System Status</div>
                <div className="text-3xl font-bold font-mono tracking-tight flex items-center gap-3">
                  {statusText}
                  <div className={`w-3 h-3 rounded-full ${statusColor}`} />
                </div>
              </div>
            </div>

            <div className="flex gap-4 w-full md:w-auto">
              <Button 
                size="lg" 
                variant="outline"
                className={`flex-1 md:w-32 h-14 font-mono font-bold tracking-wider transition-all duration-300 ${
                  isRunning 
                    ? 'border-destructive/50 hover:bg-destructive hover:text-destructive-foreground text-destructive' 
                    : 'opacity-50 pointer-events-none'
                }`}
                onClick={handleStop}
                disabled={!isRunning || pendingAction === "stop"}
              >
                {pendingAction === "stop" ? "STOPPING..." : "STOP"}
              </Button>
              <Button 
                size="lg" 
                className={`flex-1 md:w-40 h-14 font-mono font-bold tracking-wider transition-all duration-300 ${
                  !isRunning 
                    ? 'bg-primary hover:bg-primary/90 text-primary-foreground shadow-[0_0_20px_rgba(249,115,22,0.3)]' 
                    : 'opacity-50 pointer-events-none bg-muted text-muted-foreground'
                }`}
                onClick={handleStart}
                disabled={isRunning || pendingAction === "start" || !status}
              >
                {pendingAction === "start" ? "STARTING..." : "START"}
              </Button>
            </div>
          </CardContent>
        </Card>

        {/* Parameters */}
        <div className="col-span-1 md:col-span-3 grid grid-cols-1 md:grid-cols-2 gap-6">
          <ParamControl
            title="Frequency"
            icon={<Activity className="w-4 h-4 text-primary" />}
            value={params.f}
            min={1000}
            max={400000}
            step={100}
            unit="Hz"
            onChange={(v) => updateParam('f', v)}
            actualValue={status?.freq}
          />
          <ParamControl
            title="Duty Cycle"
            icon={<Gauge className="w-4 h-4 text-primary" />}
            value={params.d}
            min={0}
            max={95}
            step={1}
            unit="%"
            onChange={(v) => updateParam('d', v)}
            actualValue={status?.duty}
          />
          <ParamControl
            title="Dead Time"
            icon={<Clock className="w-4 h-4 text-primary" />}
            value={params.dt}
            min={0}
            max={5000}
            step={10}
            unit="ns"
            onChange={(v) => updateParam('dt', v)}
            actualValue={status?.dt}
          />
          <ParamControl
            title="Soft Start"
            icon={<Timer className="w-4 h-4 text-primary" />}
            value={params.ss}
            min={500}
            max={10000}
            step={100}
            unit="ms"
            onChange={(v) => updateParam('ss', v)}
            actualValue={status?.ss}
          />
        </div>
      </div>
    </div>
  );
}

function ParamControl({ 
  title, 
  icon, 
  value, 
  min, 
  max, 
  step, 
  unit, 
  onChange,
  actualValue
}: { 
  title: string; 
  icon: React.ReactNode; 
  value: number; 
  min: number; 
  max: number; 
  step: number; 
  unit: string; 
  onChange: (val: number) => void;
  actualValue?: number;
}) {
  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    let val = parseFloat(e.target.value);
    if (isNaN(val)) return;
    if (val < min) val = min;
    if (val > max) val = max;
    onChange(val);
  };

  return (
    <Card className="bg-card/50 border-border">
      <CardContent className="p-5 space-y-5">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            {icon}
            <Label className="text-sm font-medium font-mono text-muted-foreground tracking-wide uppercase">{title}</Label>
          </div>
          <div className="flex items-center gap-2">
            <Input 
              type="number" 
              value={value} 
              onChange={handleInputChange}
              min={min}
              max={max}
              step={step}
              className="w-24 h-8 text-right font-mono font-bold bg-background border-border"
            />
            <span className="text-xs font-mono text-muted-foreground w-6">{unit}</span>
          </div>
        </div>
        
        <div className="pt-2">
          <Slider
            value={[value]}
            min={min}
            max={max}
            step={step}
            onValueChange={(vals) => onChange(vals[0])}
            className="py-2"
          />
          <div className="flex justify-between mt-2 text-[10px] font-mono text-muted-foreground opacity-50">
            <span>{min}</span>
            <span>{max}</span>
          </div>
        </div>

        {actualValue !== undefined && (
          <div className="flex justify-between items-center pt-3 mt-1 border-t border-border/50">
            <span className="text-xs font-mono text-muted-foreground">Live Reading</span>
            <span className="text-sm font-mono font-bold text-primary">{actualValue} {unit}</span>
          </div>
        )}
      </CardContent>
    </Card>
  );
}
