# مشروع منظم السخان الحثي (Induction Heater Controller Project)

هذا المشروع عبارة عن نظام متكامل للتحكم في سخان حثي باستخدام متحكم ESP32، مع واجهة تحكم ويب متطورة وخادم خلفي (API).

## هيكل المشروع (Project Structure)

يستخدم المشروع نظام المستودع الموحد (Monorepo) باستخدام `pnpm`:

- **`artifacts/`**: التطبيقات الأساسية.
  - `api-server`: خادم Node.js (Express) للتحكم والإدارة.
  - `induction-controller`: واجهة الويب (React + Vite) للتحكم في السخان.
  - `mockup-sandbox`: بيئة تجريبية للمكونات.
- **`lib/`**: المكتبات المشتركة.
  - `api-client-react`: عميل API لـ React.
  - `api-spec`: تعريف OpenAPI والرموز المولدة.
  - `api-zod`: مخططات التحقق (Zod).
  - `db`: قاعدة البيانات (Drizzle ORM).
- **`firmware/`**: الكود البرمجي لمتحكم ESP32 (Arduino framework).
- **`scripts/`**: أدوات ومساعدات برمجية.

## المتطلبات (Requirements)

- **Node.js**: الإصدار 24 أو أحدث.
- **pnpm**: الإصدار 10 أو أحدث.
- **Arduino IDE / PlatformIO**: لبرمجة متحكم ESP32.

## كيفية التشغيل (How to Run)

1. **تثبيت التبعيات**:
   ```bash
   pnpm install
   ```

2. **تشغيل خادم الـ API**:
   ```bash
   pnpm --filter @workspace/api-server run dev
   ```

3. **تشغيل واجهة التحكم**:
   ```bash
   pnpm --filter @workspace/induction-controller run dev
   ```

4. **التحقق من الأنواع (Typecheck)**:
   ```bash
   pnpm run typecheck
   ```

5. **بناء المشروع**:
   ```bash
   pnpm run build
   ```

## البناء عبر GitHub (GitHub Build)

تم إعداد المشروع ليتم بناؤه والتحقق منه تلقائياً عبر **GitHub Actions** عند كل عملية دفع (Push) إلى فرع `main`.

---

# Induction Heater Controller Project

A comprehensive system to control an induction heater using ESP32, featuring an advanced web interface and a backend API.

## Project Structure

The project uses a pnpm monorepo:

- **`artifacts/`**: Main applications.
- **`lib/`**: Shared libraries.
- **`firmware/`**: ESP32 source code (Arduino).
- **`scripts/`**: Utility scripts.

## How to Build

The project is configured with GitHub Actions to automatically build and typecheck on every push.
