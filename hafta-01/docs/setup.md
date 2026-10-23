# Kurulum ve yapılandırma (Faz 0b)

Bu projede **STM32CubeMX kullanılmıyor**. CubeMX'in `.ioc` dosyasından üreteceği her şey
(saat ağacı, pinler, NVIC, DMA, HAL timebase, FreeRTOS ayarı) `firmware/Core/` altında elle
yazıldı. Bu belge her ayarın **CubeMX'te olsaydı nereye karşılık geleceğini** gösterir.

---

## 1. Araçlar

Hepsi STM32CubeIDE 2.2.0 kurulumunun içinde gelir. Ayrıca bir kurulum gerekmez.

| Araç | Sürüm | Konum (`C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\...`) |
|---|---|---|
| arm-none-eabi-gcc | GNU Tools for STM32 14.3.rel1 (GCC 14.3.1) | `...gnu-tools-for-stm32.14.3.rel1.win32_*/tools/bin` |
| CMake | 4.3.1 | `...externaltools.cmake.win32_*/tools/bin` |
| Ninja | 1.13.2 | `...externaltools.ninja.win32_*/tools/bin` |

Proje, CubeIDE 2.2'nin **"STM32 CMake project"** şablonuyla oluşturuldu (ad: `TimingHomework`,
MCU: `STM32F407VGTx`). Derleme sistemi CMake + Ninja'dır; CubeIDE de arka planda
`cmake --preset Debug` çalıştırır.

## 2. Sürücüler (git submodule, sabit etiket)

| Klasör (`firmware/third_party/`) | Depo | Etiket | Commit |
|---|---|---|---|
| `stm32f4xx-hal-driver` | github.com/STMicroelectronics/stm32f4xx-hal-driver | **v1.8.5** | `b6f0ed3` |
| `cmsis-device-f4` | github.com/STMicroelectronics/cmsis-device-f4 | **v2.6.11** | `0fa0e48` |
| `cmsis-core` | github.com/STMicroelectronics/cmsis-core | **v5.6.0_cm4** | `be0e91f` |
| `FreeRTOS-Kernel` | github.com/FreeRTOS/FreeRTOS-Kernel | **V11.3.1** | `3a22924` |

**Neden bu etiketler:**
- **HAL v1.8.5:** ST'nin resmi paketi STM32CubeF4 v1.28.3'ün sabitlediği commit ile birebir aynı.
- **cmsis-device-f4 v2.6.11:** CubeF4 v1.28.3, v2.6.11'den sonraki tek bir commit'i kullanıyor. O commit
  sadece `IRQn_Type` içindeki boşlukları düzeltiyor, işlevsel fark yok.
- **CMSIS Core v5.6.0_cm4:** CubeF4 v1.28.3 de CMSIS Core 5.6 dağıtıyor. HAL ve device başlıkları bu
  sürümle test edilmiş.
- **FreeRTOS V11.3.1:** En son kararlı sürüm (2026-08).

Dört submodule de `shallow = true` (sadece etiketlenen commit indirilir). FreeRTOS deposunun tam
geçmişi ~128 MB tutuyor.

```bash
git submodule update --init --recursive
```

Repoyu klonladıktan sonra bu komut **zorunlu**. Çalıştırılmazsa derleme
`stm32f4xx_hal.h: No such file` hatasıyla durur.

## 3. Derleme

**CubeIDE:** Proje → Build Project. Çıktı `firmware/Debug/TimingHomework.elf`, debug/yükleme yine
CubeIDE'den yapılır.

**Komut satırı (Git Bash):** CubeIDE'nin araçları PATH'e sadece o oturum için eklenir.

```bash
P=/c/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins; export PATH="$(ls -d $P/*gnu-tools-for-stm32*.win32_*/tools/bin):$(ls -d $P/*externaltools.cmake.win32_*/tools/bin):$(ls -d $P/*externaltools.ninja.win32_*/tools/bin):$PATH"
```

```bash
cd firmware && cmake --preset Debug && cmake --build --preset Debug
```

Komut satırı ve CubeIDE **aynı `Debug/` klasörünü ve aynı derleyiciyi** kullanır. Bu yüzden ikisi
birbirinin önbelleğini bozmaz ve CubeIDE'nin debug ettiği ELF, komut satırının ürettiği ELF ile aynıdır.

Faz 0b sonu boyut (Debug, `-O0 -g`):
```
   text    data     bss     dec     hex filename
  73484      16   32872  106372   19f84 TimingHomework.elf
```
`bss`'in 30 720 baytı FreeRTOS heap_4 dizisidir (`ucHeap`).

### CMakeLists.txt'e eklenenler
CubeIDE şablonunun yapısı korundu. Sadece `VARIABLES` bölümündeki listeler dolduruldu:
- `MFLOAT_ABI = -mfloat-abi=hard`. `MFPU` zaten `-mfpu=fpv4-sp-d16` idi.
- `PROJECT_SOURCES`:
  - `Core/Src/*`
  - `system_stm32f4xx.c` (submodule şablonundan, değiştirilmeden)
  - 11 HAL modülü: `hal, rcc, rcc_ex, gpio, cortex, uart, dma, tim, tim_ex, pwr, flash`
  - 5 FreeRTOS dosyası: `tasks, queue, list, ARM_CM4F/port, heap_4`
- `PROJECT_DEFINES = STM32F407xx USE_HAL_DRIVER HSE_VALUE=8000000`
- `PROJECT_INCLUDES`:
  - `Core/Inc`
  - HAL `Inc`
  - CMSIS device `Include`
  - CMSIS `Core/Include`
  - FreeRTOS `include` ve `portable/GCC/ARM_CM4F`

Derleme `-Wall -Werror` ile sıfır uyarıyla geçiyor. Üçüncü taraf kod için ayrıca istisna
gerekmedi. `stm32f4xx_hal_flash_ex.c` eklenmedi: `hal_flash.c`'deki fonksiyonları kimse çağırmadığı
için `--gc-sections` onları atıyor, link hatası oluşmuyor.

---

## 4. Dosya haritası: CubeMX ne üretirdi, bizde ne var

| CubeMX çıktısı | Bu projede | Not |
|---|---|---|
| `Core/Src/main.c` (`main`, `SystemClock_Config`, `Error_Handler`) | `main.c` + `system_clock.c` | Saat ayrı modüle alındı |
| `Core/Src/gpio.c`, `dma.c`, `usart.c`, `tim.c` | Aynı adlar | CubeMX: *Project Manager → Code Generator → "Generate peripheral initialization as a pair of '.c/.h' files per peripheral"* |
| `Core/Src/stm32f4xx_hal_msp.c` | Aynı ad | Sadece `HAL_MspInit`; çevre birimi MspInit'leri kendi dosyalarında |
| `Core/Src/stm32f4xx_hal_timebase_tim.c` | Aynı ad | *SYS → Timebase Source = TIM6* seçilince üretilir |
| `Core/Src/stm32f4xx_it.c` | Aynı ad | SVC/PendSV/SysTick yok (bkz. §6) |
| `Core/Src/freertos.c` | `freertos_hooks.c` | Sadece hook'lar; CMSIS-RTOS sarmalayıcısı yok |
| `Core/Inc/FreeRTOSConfig.h` | Aynı ad | Elle yazıldı |
| `Core/Inc/stm32f4xx_hal_conf.h` | Aynı ad | Sadece kullanılan modüller açık |
| `Core/Src/system_stm32f4xx.c` (kopya) | Submodule'den doğrudan derleniyor | Değişiklik gerekmediği için kopya tutulmadı |
| `Core/Startup/startup_stm32f407vgtx.s` | `Startup/startup_stm32f407vgtx.s` | CubeIDE proje oluştururken üretti; tek startup dosyası |
| `STM32F407VGTX_FLASH.ld` | Aynı ad | CubeIDE üretti, değiştirilmedi |
| `Core/Src/syscalls.c`, `sysmem.c` | Aynı ad | CubeIDE üretti, `Sources/`'tan `git mv` ile taşındı |

---

## 5. CubeMX karşılık tabloları

### 5.1 Saat ağacı — *Clock Configuration* sekmesi + *System Core → RCC*

| Ayar | Değer | Bizim kod | CubeMX'te nerede |
|---|---|---|---|
| HSE kaynağı | Kristal 8 MHz (Discovery X2) | `RCC_HSE_ON`, `-DHSE_VALUE=8000000` | *RCC → High Speed Clock (HSE) = Crystal/Ceramic Resonator*; Clock sekmesinde *Input frequency = 8* |
| PLL kaynağı | HSE | `PLLSource = RCC_PLLSOURCE_HSE` | Clock sekmesi *PLL Source Mux* |
| PLLM | /8 → VCO girişi 1 MHz | `PLLM = 8` | */M* kutusu |
| PLLN | ×336 → VCO 336 MHz | `PLLN = 336` | **N* kutusu |
| PLLP | /2 → **SYSCLK 168 MHz** | `RCC_PLLP_DIV2` | */P* kutusu |
| PLLQ | /7 → 48 MHz (USB/SDIO/RNG) | `PLLQ = 7` | */Q* kutusu |
| SYSCLK kaynağı | PLLCLK | `RCC_SYSCLKSOURCE_PLLCLK` | *System Clock Mux* |
| AHB | /1 → **HCLK 168 MHz** | `RCC_SYSCLK_DIV1` | *AHB Prescaler* |
| APB1 | /4 → **PCLK1 42 MHz**, APB1 timer 84 MHz | `RCC_HCLK_DIV4` | *APB1 Prescaler* (CubeMX "APB1 Timer clocks = 84" gösterir) |
| APB2 | /2 → **PCLK2 84 MHz** | `RCC_HCLK_DIV2` | *APB2 Prescaler* |
| Flash bekleme | **5 WS** | `HAL_RCC_ClockConfig(..., FLASH_LATENCY_5)` | *RCC → Parameter Settings → Flash Latency(WS)* (CubeMX otomatik hesaplar) |
| Voltaj ölçeği | Scale 1 | `__HAL_PWR_VOLTAGESCALING_CONFIG(SCALE1)` | *RCC → Parameter Settings → Power Regulator Voltage Scale* |
| Prefetch / I-cache / D-cache | Açık | `stm32f4xx_hal_conf.h` (`PREFETCH_ENABLE` vb.), `HAL_Init` uygular | *RCC → Parameter Settings → Prefetch Buffer / Instruction Cache / Data Cache* |

**Neden APB1 timer saati 84 MHz:** APB böleni 1'den farklıysa STM32F4 timer'ları PCLK'nın 2 katıyla
çalışır. TIM2 ve TIM6 APB1 üzerinde olduğu için prescaler hesabı 84 MHz'e göre yapılır.

### 5.2 Pinler — *Pinout & Configuration* → *System Core → GPIO*

| Pin | İşlev | Ayar | Bizim kod | CubeMX'te nerede |
|---|---|---|---|---|
| PA0 | B1 butonu | EXTI0, **yükselen kenar**, no-pull | `gpio.c` `GPIO_MODE_IT_RISING`, `GPIO_NOPULL` | Pine tıkla → `GPIO_EXTI0`; *GPIO → PA0 → GPIO mode = External Interrupt Mode with Rising edge trigger detection*, *Pull = No pull-up and no pull-down*, *User Label = BUTTON* |
| PD12 | Yeşil LED | Çıkış PP, başlangıç LOW, low speed | `gpio.c` | Pin → `GPIO_Output`; *Output level Low, Push Pull, No pull, Low*, *User Label = LED_GREEN* |
| PD13 / PD14 / PD15 | Turuncu / Kırmızı / Mavi LED | Aynı | `gpio.c` | Aynı |
| PA2 | USART2_TX | AF7, push-pull, no-pull, very high | `usart.c` `HAL_UART_MspInit` | *Connectivity → USART2 → Mode = Asynchronous* (pinleri otomatik atar) |
| PA3 | USART2_RX | AF7 | `usart.c` | Aynı |

CubeMX'teki *User Label*'lar `main.h`'de `#define LED_GREEN_Pin GPIO_PIN_12` gibi tanımlara
dönüşür. Biz de aynısını elle yazdık.

### 5.3 USART2 — *Connectivity → USART2 → Parameter Settings*

| Ayar | Değer | CubeMX alanı |
|---|---|---|
| Mode | Asynchronous, TX+RX | *Mode* / *Data Direction = Receive and Transmit* |
| Baud | 115200 | *Baud Rate* |
| Çerçeve | 8 bit, parity yok, 1 stop | *Word Length = 8 Bits*, *Parity = None*, *Stop Bits = 1* |
| Oversampling | 16 | *Over Sampling = 16 Samples* |
| Akış kontrolü | Yok | *Hardware Flow Control = Disable* |

### 5.4 DMA — *Connectivity → USART2 → DMA Settings → Add*

| Ayar | Değer | Bizim kod (`usart.c`) | CubeMX alanı |
|---|---|---|---|
| İstek | USART2_TX | `DMA_CHANNEL_4` | *DMA Request = USART2_TX* (stream/kanalı CubeMX seçer) |
| Stream | DMA1 Stream 6 | `DMA1_Stream6` | *Stream* sütunu |
| Yön | Bellek → çevre birimi | `DMA_MEMORY_TO_PERIPH` | *Direction* |
| Öncelik | Low | `DMA_PRIORITY_LOW` | *Priority* |
| Mod | **Normal** (circular değil) | `DMA_NORMAL` | *Mode* |
| Adres artırma | Bellek: evet, çevre: hayır | `MINC_ENABLE`, `PINC_DISABLE` | *Increment Address: Memory ☑, Peripheral ☐* |
| Veri genişliği | Byte / Byte | `*_BYTE` | *Data Width* |
| FIFO | Kapalı (direct mode) | `DMA_FIFOMODE_DISABLE` | *Use Fifo ☐* |
| Bağlantı | `huart2.hdmatx ↔ hdma_usart2_tx` | `__HAL_LINKDMA` | CubeMX otomatik üretir (`HAL_UART_MspInit` içinde) |
| DMA saati | DMA1 | `dma.c` `MX_DMA_Init` | CubeMX `dma.c` üretir ve `MX_USART2_UART_Init`'ten **önce** çağırır |

**Neden normal mod:** Her 64 baytlık mesaj tek seferlik bir aktarımdır. Circular modda DMA tamponu
sürekli yeniden gönderirdi.

**Neden USART2 kesmesi de açık:** DMA son baytı DR'ye yazdığında bayt henüz hattan çıkmamıştır.
HAL bu yüzden DMA TC kesmesinde USART TC kesmesini açar ve `HAL_UART_TxCpltCallback`'i ancak son bit
gönderilince çağırır. Bu, ölçüm noktası **t4**'tür.

### 5.5 TIM2 (zaman damgası) — *Timers → TIM2*

| Ayar | Değer | CubeMX alanı |
|---|---|---|
| Saat kaynağı | Dahili (84 MHz) | *Clock Source = Internal Clock* |
| Prescaler | 83 → 84 MHz / 84 = **1 MHz** (1 µs/sayım) | *Parameter Settings → Prescaler (PSC) = 83* |
| Sayma yönü | Yukarı | *Counter Mode = Up* |
| Periyot | 0xFFFFFFFF (32 bit tam sarma, ~71.6 dk) | *Counter Period (ARR) = 4294967295* |
| ARR preload | Kapalı | *auto-reload preload = Disable* |
| Kesme | **Yok** | *NVIC Settings → TIM2 global interrupt ☐* |
| Başlatma | `HAL_TIM_Base_Start` | CubeMX **üretmez**; `USER CODE BEGIN 2` içine elle yazılırdı |

`timer_us()` fonksiyonu `tim.h` içinde `TIM2->CNT` okur. Zaman farkları `uint32_t` işaretsiz
çıkarmayla alınır, böylece sayaç taşsa bile sonuç doğru kalır.

### 5.6 HAL timebase — *System Core → SYS → Timebase Source = TIM6*

| Ayar | Değer | Bizim kod | CubeMX'te nerede |
|---|---|---|---|
| Kaynak | TIM6 (SysTick değil) | `stm32f4xx_hal_timebase_tim.c` `HAL_InitTick` override | *SYS → Timebase Source* |
| Frekans | 1 kHz (1 MHz sayaç, ARR 999) | Prescaler APB1 timer saatinden hesaplanır | Otomatik |
| Öncelik | **15** (`TICK_INT_PRIORITY`) | `stm32f4xx_hal_conf.h` | *NVIC → "Time base: TIM6 global interrupt..." Preemption Priority* |

**Neden SysTick değil:** SysTick FreeRTOS'un tick kaynağıdır. HAL de SysTick kullansaydı iki taraf
aynı sayacın yükleme değerini ve önceliğini birbirinin üstüne yazardı. CubeMX de FreeRTOS etkinken
SysTick timebase'i seçince uyarı verir.

**Neden öncelik 15:** HAL tick'i FreeRTOS API çağırmaz, yani hangi öncelikte olacağı API kuralı
açısından serbesttir. 0 olsaydı her milisaniyede EXTI0 ISR'ını kesip t0 ölçümüne jitter eklerdi.
15 ile hiçbir ölçüm ISR'ını kesemez. HAL'in kendi şablonunda da varsayılan değer 15'tir.

### 5.7 NVIC — *System Core → NVIC*

**Priority Group:** *4 bits for pre-emption priority, 0 bits for subpriority*. `HAL_Init()`
içinde `NVIC_PRIORITYGROUP_4` olarak ayarlanır. FreeRTOS portu bunu her FromISR çağrısında
`vPortValidateInterruptPriority` içinde `configASSERT` ile kontrol eder. Grup 4 olmasının nedeni,
sub-priority bitleri olsaydı BASEPRI eşiğinin beklenenden farklı kesmeleri maskeleyebilmesidir.

| Kesme | Öncelik | FreeRTOS API çağırır mı | Nerede ayarlanıyor | CubeMX *NVIC* tablosu |
|---|---|---|---|---|
| EXTI0 (buton) | **6** | Evet (ileride `xQueueSendFromISR`) | `gpio.c` | *EXTI line0 interrupt ☑, Preemption 6, "Uses FreeRTOS functions" ☑* |
| DMA1_Stream6 | **6** | Dolaylı (HAL callback zinciri) | `dma.c` | *DMA1 stream6 global interrupt ☑, 6* |
| USART2 | **6** | Evet (`TxCpltCallback` → task notify) | `usart.c` | *USART2 global interrupt ☑, 6* (veya USART2 → NVIC Settings) |
| TIM6_DAC (HAL tick) | **15** | Hayır | `HAL_InitTick` | *Time base: TIM6 ..., 15* |
| SysTick (FreeRTOS tick) | 15 | (çekirdek) | FreeRTOS `xPortStartScheduler` | FreeRTOS etkinken CubeMX tabloda 15 gösterir |
| PendSV (bağlam değişimi) | 15 | (çekirdek) | FreeRTOS `xPortStartScheduler` | Aynı |

Öncelik haritası (sayı küçük = daha acil):

```
 0 ┐
 . │ FreeRTOS hiç maskeleyemez; bu kesmeler FromISR API ÇAĞIRAMAZ (şu an boş)
 4 ┘
 ── configMAX_SYSCALL_INTERRUPT_PRIORITY = 5  (taskENTER_CRITICAL -> BASEPRI = 5<<4)
 5    boş (bir seviye pay)
 6    EXTI0, DMA1_Stream6, USART2      <- FromISR API çağırabilir
 .
15    TIM6 (HAL tick), SysTick, PendSV  <- en düşük
```

**Neden 6, 5 değil:** 5 de kurala uyar. Arada bir seviye boş bırakıldı ki ileride "kritik bölümün
hemen üstünde ama API kullanan" bir kesme gerekirse yer olsun.

**SVC/PendSV/SysTick handler'ları:** CubeMX'te *NVIC → Code generation* sekmesinde bunların
*Generate IRQ handler* kutuları FreeRTOS etkinken kapatılır, çünkü handler'ları FreeRTOS verir.
CubeMX `SysTick_Handler`'ı yine üretip içinden `xPortSysTickHandler()` çağırır. Bunun nedeni, HAL
tick'inin de SysTick'te olabileceği konfigürasyonları desteklemesidir. Bizde HAL tick'i TIM6'da
olduğu için üçü de doğrudan eşlendi (§6).

### 5.8 FreeRTOS — *Middleware → FREERTOS*

CubeMX FreeRTOS'u **her zaman bir CMSIS-RTOS arayüzü (V1 veya V2) ile** ekler (`cmsis_os2.c`,
`osThreadNew`...). Bu projede CMSIS sarmalayıcısı yok; native API kullanılıyor (`xTaskCreate`,
`xQueueCreate`). Ayrıca CMSIS_V2 arayüzü `configMAX_PRIORITIES`'i 56'ya sabitler. Biz 5 kullanıyoruz.

| `FreeRTOSConfig.h` | Değer | CubeMX *Config parameters* / *Include parameters* |
|---|---|---|
| `configUSE_PREEMPTION` | 1 | *USE_PREEMPTION = Enabled* |
| `configTICK_RATE_HZ` | 1000 | *TICK_RATE_HZ* |
| `configMAX_PRIORITIES` | 5 | *MAX_PRIORITIES* (CMSIS_V2'de değiştirilemez) |
| `configMINIMAL_STACK_SIZE` | 128 word | *MINIMAL_STACK_SIZE* |
| `configTOTAL_HEAP_SIZE` | 30 720 B | *TOTAL_HEAP_SIZE* |
| heap şeması | heap_4 (birleştiren first-fit) | *Advanced settings → Memory Management scheme = heap_4* |
| `configCHECK_FOR_STACK_OVERFLOW` | 2 | *CHECK_FOR_STACK_OVERFLOW = Option2* |
| `configUSE_MALLOC_FAILED_HOOK` | 1 | *USE_MALLOC_FAILED_HOOK* |
| `configUSE_TASK_NOTIFICATIONS` | 1 | *USE_TASK_NOTIFICATIONS* |
| `configUSE_TIMERS` | 0 | *USE_TIMERS* (timers.c derlenmez; CMSIS-RTOS `osTimer` kullanılsaydı gerekirdi) |
| `configLIBRARY_LOWEST_INTERRUPT_PRIORITY` | 15 | *LIBRARY_LOWEST_INTERRUPT_PRIORITY* |
| `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` | 5 | *LIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY* |
| `configASSERT` | kesmeleri kapat + dur | CubeMX aynı makroyu üretir |
| `INCLUDE_xTaskDelayUntil` | 1 | *Include parameters → vTaskDelayUntil* |
| `INCLUDE_uxTaskGetStackHighWaterMark` | 1 | *Include parameters → uxTaskGetStackHighWaterMark* |
| `configUSE_TRACE_FACILITY`, `configRECORD_STACK_HIGH_ADDRESS` | 1 | *USE_TRACE_FACILITY*, *RECORD_STACK_HIGH_ADDRESS*. CubeIDE'nin FreeRTOS Task List görünümü için |
| `configQUEUE_REGISTRY_SIZE` | 8 | *QUEUE_REGISTRY_SIZE*. Debugger'da kuyruk adları |

---

## 6. Handler eşlemesi: neden çift tanım yok

Startup dosyası her vektörü **zayıf (weak)** olarak `Default_Handler`'a yönlendirir. Aynı adla
güçlü bir tanım varsa linker onu seçer.

```c
/* FreeRTOSConfig.h */
#define vPortSVCHandler      SVC_Handler
#define xPortPendSVHandler   PendSV_Handler
#define xPortSysTickHandler  SysTick_Handler
```

`port.c` bu başlığı içerdiği için fonksiyonları doğrudan `SVC_Handler` vb. adlarıyla derlenir.
`stm32f4xx_it.c` bu üçünü **tanımlamaz**; tanımlasaydı linker "multiple definition" hatası verirdi.
FreeRTOS 11, scheduler başlarken vektör tablosundaki SVC ve PendSV girdilerinin gerçekten port
fonksiyonlarını gösterdiğini `configCHECK_HANDLER_INSTALLATION` ile ayrıca doğrular.

Derleme sonrası doğrulama (`arm-none-eabi-nm` ve `.isr_vector` dökümü):

| Vektör | Adres | Kaynak |
|---|---|---|
| SVC (11) | 0x08010f50 | `port.c` |
| PendSV (14) | 0x08011268 | `port.c` |
| SysTick (15) | 0x080112c6 | `port.c` |
| EXTI0 (22) | 0x08000672 | `stm32f4xx_it.c` |
| DMA1_Stream6 (33) | 0x08000680 | `stm32f4xx_it.c` |
| USART2 (54) | 0x08000694 | `stm32f4xx_it.c` |
| TIM6_DAC (70) | 0x080006a8 | `stm32f4xx_it.c` |
| `Default_Handler` | 0x08011e98 | Yukarıdakilerin hiçbiri buraya düşmüyor |

---

## 7. Açılış sırası

```
Reset_Handler (startup)
 ├─ SP = _estack
 ├─ SystemInit()            FPU: SCB->CPACR CP10/CP11 tam erişim
 ├─ .data kopyala, .bss sıfırla, __libc_init_array
 └─ main()
     ├─ HAL_Init()          prefetch/cache, NVIC grup 4,
     │                      HAL_InitTick(15) -> TIM6 (HSI 16 MHz ile), HAL_MspInit
     ├─ SystemClock_Config  HSE+PLL -> 168 MHz; HAL_RCC_ClockConfig içinden
     │                      HAL_InitTick tekrar -> TIM6 prescaler 84 MHz'e göre
     ├─ MX_GPIO_Init        PD12-15, PA0/EXTI0 (NVIC 6)
     ├─ MX_DMA_Init         DMA1 saati, Stream6 NVIC 6
     ├─ MX_USART2_UART_Init PA2/PA3 AF7, DMA bağlantısı, USART2 NVIC 6
     ├─ MX_TIM2_Init        1 MHz serbest sayaç başlar
     ├─ HAL_UART_Transmit   "Merhaba F407\r\n" (polling, 100 ms timeout)
     ├─ xTaskCreate         LedBlink (öncelik 1, 128 word)
     └─ vTaskStartScheduler idle görevi, PendSV/SysTick = 15, SysTick 1 kHz, SVC -> ilk görev
```

## 8. Bilinen tuzaklar

1. **Scheduler öncesi HAL tick durur.** Scheduler başlamadan yapılan ilk FreeRTOS çağrısı
   (`xTaskCreate` gibi) `BASEPRI`'yi 5'e çeker ve scheduler başlayana kadar bırakmaz. TIM6 (15)
   maskelenir, `HAL_GetTick()` donar. Bu aralıkta `HAL_Delay` veya timeout'lu HAL çağrısı **kilitlenir**.
   Bu yüzden "Merhaba" gönderimi `xTaskCreate`'ten önce yapılıyor.
2. **EXTI0 scheduler'dan önce açık.** Faz 0b'de `HAL_GPIO_EXTI_Callback` boş (weak). Sonraki fazda
   ISR `xQueueSendFromISR` çağıracaksa kuyruk, kesme açılmadan **önce** oluşturulmalı.
3. **Öncelik < 5 olan ISR'da FromISR API** → `vPortValidateInterruptPriority` içindeki
   `configASSERT` tetiklenir.
4. **ISR içinde `HAL_Delay`** asla kullanılmamalı. TIM6 en düşük öncelikte olduğu için tick
   artmaz, sonsuz döngü olur.
5. **Submodule'ler başlatılmadıysa** include hataları alınır (§2).

## 9. UART bağlantısı ve duman testi

Kart revizyonuna göre ST-LINK'in sanal COM portu olmayabilir veya MCU'nun USART2'sine bağlı
olmayabilir (UM1472'ye bak). En güvenli yol **3.3 V USB-TTL dönüştürücü**:

| Kart | Dönüştürücü |
|---|---|
| PA2 (USART2_TX) | RX |
| PA3 (USART2_RX) | TX |
| GND | GND |

Terminal: 115200 8N1.

**Beklenen davranış:**
- Reset sonrası bir kez `Merhaba F407` görünür.
- Yeşil LED (PD12) 500 ms yanık / 500 ms sönük, yani 1 Hz yanıp söner.

### Faz 0b donanım testi sonucu (2026-10-04, STM32CubeIDE debugger + ST-LINK)

| Test | Sonuç | Neyi kanıtlar |
|---|---|---|
| Derleme (CubeIDE) | ✅ | Yapılandırma ve sürücüler tutarlı |
| Yeşil LED 1 Hz | ✅ | Yükleme, FreeRTOS scheduler, `xTaskDelayUntil` |
| `SystemCoreClock`: 16 000 000 → 168 000 000 | ✅ | HSE + PLL saat ağacı |
| `HAL_UART_Transmit` sonrası satıra ulaşıldı | ✅ | USART2 14 baytı gönderdi (TC bayrağı beklendi) |
| "Merhaba F407" PC'de görüldü | ✅ (2026-10-12) | USART2 → PA2 → CP2102 USB-TTL dönüştürücü → PC, 115200 8N1 |

Karttaki ST-LINK sanal COM portu (COM5) var ama PA2'ye bağlı değil; bu yüzden UART çıkışı
3.3 V USB-TTL dönüştürücüyle alındı (PA2 → RX, GND → GND, VCC bağlanmadı). CP2102 için
Windows'ta Silicon Labs CP210x VCP sürücüsü gerekti (sürücüsüz cihaz hata kodu 28 verir ve
COM portu olarak görünmez).

## 10. Faz 1–6 sonrası güncel durum

Yukarıdaki bölümler Faz 0b iskeletini anlatır. Son haldeki farklar ve
eklenenler aşağıda. Kodun açıklaması `docs/code-notes.md`'de.

**Uygulama modülleri (`firmware/App/`):** `app_time` (TIM2 zaman damgası),
`app_button` (buton kesmesi, filtre, ButtonTask), `app_msg` (64 baytlık
mesaj), `app_record` (kayıt havuzu), `app_telemetry` (TelemetryTask,
kalibrasyon), `app_uart_tx` (UartTxTask, UART callback'leri), `app_tasks`
(kuyruk ve görevlerin oluşturulması), `app_experiment` (deney akışı ve
dışa aktarma). `app_experiment.c` Faz 4'te `CMakeLists.txt`'e eklendi.

**Görevler ve kuyruklar:**

| Görev | Öncelik | Yığın |
|---|---|---|
| TelemetryTask | 3 | 256 word (1 KB) |
| ButtonTask | 2 | 256 word (1 KB) |
| UartTxTask | 1 | 512 word (2 KB): dışa aktarmada uzun CNT satırını biçimlendirir |

`buttonQ`: 8 × `ButtonEvent` (8 B). `txQ`: 16 × `TxMsg` (72 B). Heap: heap_4,
30 KB.

**Kesme öncelikleri (NVIC, küçük sayı = yüksek öncelik):**

| Kesme | Öncelik | Not |
|---|---|---|
| EXTI0 (buton), USART2, DMA1 Stream6 | 6 | `FromISR` çağırırlar; ≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5) |
| TIM6 (HAL tick + buton bırakma takibi) | 15 | Ölçüm kesmelerini kesmesin |
| SysTick, PendSV (FreeRTOS) | 15 | |

**Deney ayarları (`firmware/App/Inc/app_config.h`):**

| Ayar | Değer |
|---|---|
| `SCENARIO` | 0–5, derleme anında seçilir (varsayılan 1) |
| `WORK_ITERATIONS_2MS` / `_5MS` | 15 952 / 39 880 (kalibrasyon: 7976 iter/ms, Debug -O0; 2026-10-20) |
| `EXPERIMENT_WARMUP_MS` | 5000 |
| `EXPERIMENT_TARGET_EVENTS` | 30 |
| `EXPERIMENT_END_DELAY_MS` | 1000 |
| `UART_TX_TIMEOUT_MS` | 1000 (gözetim timeout'u) |

S4/S5, iterasyonlar 0 iken bilerek derlenmez. Kalibrasyon optimizasyon
seviyesine bağlıdır; derleme ayarı değişirse açılıştaki `CAL` satırından
yeniden hesapla.

**LED'ler:** 9. bölümdeki "yeşil LED 1 Hz" davranışı Faz 0b'nin test
göreviydi. Son halde:
- Yeşil (PD12): her BTN yanıtında durum değiştirir.
- Turuncu (PD13): CPU işi sürerken yanar; S4'te soluk, S5'te daha parlak.

**PC araçları:** Python 3.13.2, pyserial 3.5, matplotlib 3.11.1, tkinter 8.6
(`interface/requirements.txt`).

**Bilinen tuzak:** STM32CubeIDE 2.2, projeyi açıp derleyince
`firmware/.cproject` içindeki kaynak yolunu (`.`) bilgisayara özgü mutlak bir
yola çevirebiliyor. Bu değişikliği commit etme: o yol başka bir bilgisayarda
var olmayan bir klasörü gösterir.
