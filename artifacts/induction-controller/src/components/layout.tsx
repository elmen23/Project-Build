import { ReactNode, useEffect } from "react";
import { Link, useLocation } from "wouter";
import { useEsp32Context } from "@/lib/esp32-context";
import { AlertCircle, X, Activity, Settings2, Wifi } from "lucide-react";
import { Button } from "@/components/ui/button";

export function Layout({ children }: { children: ReactNode }) {
  const { ip, isOnline, connectionError, setConnectionError } = useEsp32Context();
  const [location] = useLocation();

  // Force dark mode on html tag
  useEffect(() => {
    document.documentElement.classList.add('dark');
  }, []);

  return (
    <div className="min-h-screen bg-background text-foreground flex flex-col font-sans selection:bg-primary selection:text-primary-foreground">
      {/* Top Banner for connection errors */}
      {connectionError && (
        <div className="bg-destructive/10 border-b border-destructive/20 p-3 px-4 flex items-center justify-between">
          <div className="flex items-center gap-3 text-destructive">
            <AlertCircle className="w-5 h-5" />
            <span className="text-sm font-medium">{connectionError}</span>
          </div>
          <div className="flex items-center gap-2">
            <Button variant="outline" size="sm" className="h-8 border-destructive/20 hover:bg-destructive/10 text-destructive text-xs" onClick={() => window.location.reload()}>
              Retry
            </Button>
            <Button variant="ghost" size="icon" className="h-8 w-8 text-muted-foreground hover:text-foreground" onClick={() => setConnectionError(null)}>
              <X className="w-4 h-4" />
            </Button>
          </div>
        </div>
      )}

      {/* Header */}
      <header className="border-b border-border bg-card/50 backdrop-blur-sm sticky top-0 z-10">
        <div className="container max-w-5xl mx-auto px-4 h-16 flex items-center justify-between">
          <div className="flex items-center gap-6">
            <Link href="/" className="flex items-center gap-3 hover:opacity-80 transition-opacity">
              <div className="bg-primary/10 p-2 rounded-md">
                <Activity className="w-5 h-5 text-primary" />
              </div>
              <div>
                <h1 className="font-bold text-sm tracking-tight text-foreground">INDUCTION</h1>
                <p className="text-[10px] text-muted-foreground font-mono tracking-wider">CONTROLLER v2.0</p>
              </div>
            </Link>
          </div>

          <div className="flex items-center gap-4">
            <div className="hidden sm:flex items-center gap-2 bg-secondary/50 px-3 py-1.5 rounded-md border border-border">
              <div className={`w-2 h-2 rounded-full ${isOnline ? 'bg-green-500' : 'bg-red-500'}`} />
              <span className="text-xs font-mono text-muted-foreground">{ip}</span>
            </div>
            
            <nav className="flex items-center gap-1">
              <Link href="/setup">
                <Button variant={location === "/setup" ? "secondary" : "ghost"} size="sm" className="gap-2 h-9 text-xs">
                  <Wifi className="w-4 h-4" />
                  <span className="hidden sm:inline">WiFi Setup</span>
                </Button>
              </Link>
              <Link href="/settings">
                <Button variant={location === "/settings" ? "secondary" : "ghost"} size="icon" className="h-9 w-9">
                  <Settings2 className="w-4 h-4 text-muted-foreground" />
                </Button>
              </Link>
            </nav>
          </div>
        </div>
      </header>

      {/* Main Content */}
      <main className="flex-1 container max-w-5xl mx-auto p-4 py-8">
        {children}
      </main>
    </div>
  );
}
