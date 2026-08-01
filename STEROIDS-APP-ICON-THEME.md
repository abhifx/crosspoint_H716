# STEROIDS — App Icons & Theme Guide

Guida operativa per aggiungere una nuova app o un'icona nel firmware CPR-vCodex
("Steroids") così che sia **visibile in TUTTI i temi** e che le impostazioni di
**ordinamento/visibilità / posizione** delle app vengano **salvate in modo
persistente** tra i riavvii.

Questa guida raccoglie i requisiti che devono essere tutti verificati: se ne
manca uno, l'icona può risultare invisibile in Home oppure l'ordinamento può
resettare al riavvio.

---

## Indice

1. [Flusso dati di un'icona app](#1-flusso-dati-di-unicona-app)
2. [Aggiungere una nuova icona (sprite bitmap)](#2-aggiungere-una-nuova-icona-sprite-bitmap)
3. [Registrare l'app in ShortcutRegistry](#3-registrare-lapp-in-shortcutregistry)
4. [Mappare l'icona in TUTTI i temi](#4-mappare-icona-in-tutti-i-temi)
5. [Persistenza ordinamento / visibilità / posizione](#5-persistenza-ordinamento--visibilità--posizione)
6. [Checklist di verifica](#6-checklist-di-verifica)
7. [File coinvolti](#7-file-coinvolti)

---

## 1. Flusso dati di un'icona app

Quando un'app appare in Home o nella griglia Apps, il percorso è:

```
ShortcutRegistry (ShortcutDefinition)
        |            +-- UIIcon enum value (BaseTheme.h)
        |            +-- location/order/visible ptr (CrossPointSettings.h)
        v
Home / ShortcutOrderActivity
        |
        v
Theme::drawIcon / renderer.drawIcon(...)
        |
        v
Theme iconForName(UIIcon icon, [size])  ->  const uint8_t* bitmap  (o nullptr)
        |
        v
renderer.drawIcon(bitmap, x, y, w, h)
```

Quindi per far comparire un'app servono **esattamente 5 pezzi**:

1. Lo **sprite bitmap** (header `.h` con l'array `static const uint8_t`).
2. Un valore **nell'enum `UIIcon`** (`BaseTheme.h`).
3. Una riga in **`ShortcutRegistry.h`** (`ShortcutDefinition`) con icona e puntatori
   alle impostazioni ordine/visibilità/posizione.
4. Un **case in `iconForName()` di OGNI tema attivo** (altrimenti `nullptr` →
   icona invisibile).
5. La **persistenza JSON** delle 3 impostazioni dell'app in `JsonSettingsIO.cpp`
   (load **e** save), altrimenti ordinamento/visibilità NON si salvano.

---

## 2. Aggiungere una nuova icona (sprite bitmap)

1. Genera (o converti) il PNG in un array binario bitmap 1-bpp per lo standard del
   progetto. Di norma esistono due dimensioni per le icone app:
   - **32x32** → file `components/icons/<name>icon.h`, simbolo `static const uint8_t <Name>Icon[]`
   - **24x24** → file `components/icons/<name>icon24.h`, simbolo `static const uint8_t <Name>24Icon[]`
   - Esempi esistenti: `wikipediaicon.h` (`WikipediaIcon`, 32) e
     `wikipediaicon24.h` (`Wikipedia24Icon`, 24).

2. Il file deve avere il formato:
   ```cpp
   #pragma once
   #include <cstdint>
   // size: 32x32
   static const uint8_t WikipediaIcon[] = { ... };
   ```

3. I file bitmap si trovano in `src/components/icons/`.

> Nota: le icone 24/32 usate dalle app in Home/grill sono tipicamente 32px. I
> vecchi temi (Classic o `TextInd`/`ScreenSaver`/`Pageview`) potrebbero usare
> dimensioni 24px; controlla il tema di destinazione.

---

## 3. Registrare l'app in ShortcutRegistry

Oltre all'icona bitmap, un'app è un *shortcut*. Tutta la registrazione avviene in:

- **`src/util/ShortcutRegistry.h`**

Passi:

1. Aggiungi un valore all'enum interno dello shortchiuto se nuovo:
   ```cpp
   enum class ShortcutId {
     ...
     Wikipedia,
   };
   ```

2. Aggiungi la voce `ShortcutDefinition` nell'array `getShortcutDefinitions()`:
   ```cpp
   ShortcutDefinition{ShortcutId::Wikipedia, StrId::STR_WIKIPEDIA, StrId::STR_WIKIPEDIA_APP_DESC,
                      UIIcon::Wikipedia,
                      &CrossPointSettings::wikipediaShortcut,        // location
                      &CrossPointSettings::wikipediaShortcutOrder,   // order
                      &CrossPointSettings::wikipediaShortcutVisible} // visible
   ```
   I tre puntatori devono riferirsi a campi di `CrossPointSettings` (passo 5).

3. L'array ha una dimensione fissa: `.size() + 1` è usato come tetto per gli
   ordini. Quando aggiungi una voce, **il conteggio è automatico** (array `std::array`),
   ma verifica che `ShortcutId` e il numero di voci restino coerenti.

---

## 4. Mappare l'icona in TUTTI i temi

Il punto dove "l'icona sparisce" è quasi sempre questa tabella: nei temi il
`switch` su `UIIcon` deve avere il `case`, altrimenti il default ritorna
`nullptr` e l'app è invisibile.

Temi soggetti a questo problema (vanno aggiornati tutti):

| File | Funzione | Dimensioni |
|------|----------|------------|
| `src/components/themes/lyra/LyraTheme.cpp` | `iconForName(UIIcon)`, 2 blocchi (24 e 32) | 24 + 32 |
| `src/components/themes/lyra/LyraCarouselTheme.cpp` | `iconForName(UIIcon, int size)` | 24 + 32 |
| `src/components/themes/lyra/LyraMarcoand75Theme.cpp` | `iconForName(UIIcon)` | 32 (mappa tutte le app) |
| `src/components/themes/lyra/LyraCustomTheme.cpp` | (eredita) | — |

**Regola pratica:** per ogni tema che ha una `iconForName` propria PRIVATA,
aggiungi:
```cpp
case UIIcon::Wikipedia:
  return WikipediaIcon;   // per il blocco 32px
// e
case UIIcon::Wikipedia:
  return Wikipedia24Icon; // se esiste un blocco 24px
```
e aggiungi l'`#include "components/icons/wikipediaicon.h"` (e `wikipediaicon24.h`
se usi la variante 24) in quel file.

> Confronto storico utile: i commit
> `47a18ae640d2dfc4d9b94c57feb9bfab001060f5` e
> `51862b261b94c2a62bb88491049eef9d02cde489` mostrano esattamente come un
> mapping mancante rendeva invisibili le icone (25+ icone mancanti nel Carousel
> theme, e il caso `UIIcon::File -> ClipIcon32` mancante nel blocco 32px). Lo
> stesso pattern vale per la mappatura di Wikipedia in `LyraMarcoand75Theme`.

---

## 5. Persistenza ordinamento / visibilità / posizione

Perché ordinamento, visibilità e posizione (Home vs Apps) vengano salvate tra i
riavvii, i campi di ogni app devono essere **serializzati** in
`JsonSettingsIO.cpp`. Se un'app è in `ShortcutRegistry` ma NON in
`JsonSettingsIO`, i valori tornano ai default a ogni boot.

Per ogni app servono **3 campi in `CrossPointSettings.h`**:
```cpp
uint8_t wikipediaShortcut = SHORTCUT_APPS;  // posizione Home/Apps
uint8_t wikipediaShortcutOrder = 22;        // ordine (default: ultimo)
uint8_t wikipediaShortcutVisible = 1;       // visibile
```

E **3 righe in `JsonSettingsIO.cpp`**, in **entrambi** i punti load (sono 2 di
solito: `loadSettings` + un eventuale reload) e nel punto save:

### Load (ripeti in OGNI funzione di caricamento, es. righe ~670 e ~1290)
```cpp
s.wikipediaShortcut = clamp(doc["wikipediaShortcut"] | s.wikipediaShortcut, shortcutLocationCount, s.wikipediaShortcut);
s.wikipediaShortcutOrder = clamp(doc["wikipediaShortcutOrder"] | s.wikipediaShortcutOrder, shortcutOrderCount, s.wikipediaShortcutOrder);
s.wikipediaShortcutVisible = clamp(doc["wikipediaShortcutVisible"] | s.wikipediaShortcutVisible, static_cast<uint8_t>(2), s.wikipediaShortcutVisible);
```

### Save (~riga 1040)
```cpp
doc["wikipediaShortcut"] = s.wikipediaShortcut;
doc["wikipediaShortcutOrder"] = s.wikipediaShortcutOrder;
doc["wikipediaShortcutVisible"] = s.wikipediaShortcutVisible;
```

### Limiti di clamp
- `shortcutLocationCount = CrossPointSettings::SHORTCUT_LOCATION_COUNT`
  (es. 0=Home, 1=Apps).
- `shortcutOrderCount = getShortcutDefinitions().size() + 1`
  (con 21 definizioni vale 22).

> Importante: un'ordine predefinito `22` (== `shortcutOrderCount`) è il **più
> alto** → l'app compare sempre in **ultima posizione** finché l'utente non la
> riordina. Se vuoi che una nuova app parta in mezzo, assegna un valore minore.

---

## 6. Checklist di verifica

Quando aggiungi un'app o un'icona, spunta TUTTO:

1. ☐ Sprite bitmap presente in `src/components/icons/` (es. `wikipediaicon.h`).
2. ☐ Valore aggiunto all'enum `UIIcon` in `BaseTheme.h` (se è un'icona nuova).
3. ☐ `ShortcutDefinition` presente in `ShortcutRegistry.h::getShortcutDefinitions()`.
4. ☐ 3 campi (`...Shortcut`, `...ShortcutOrder`, `...ShortcutVisible`) in `CrossPointSettings.h`.
5. ☐ Case in **`LyraTheme.cpp`** (blocchi 24 e 32).
6. ☐ Case in **`LyraCarouselTheme.cpp`** (blocchi 24 e 32).
7. ☐ Case in **`LyraMarcoand75Theme.cpp`** (e altri temi con `iconForName` propria).
8. ☐ `#include` dell'header bitmap in **ogni** tema modificato.
9. ☐ 3 righe di **load** in `JsonSettingsIO.cpp` (in TUTTI i punti di load).
10. ☐ 3 righe di **save** in `JsonSettingsIO.cpp`.
11. ☐ `python -X utf8 -m platformio run -e default -j 16` compila.
12. ☐ Test su device: apre l'app da Home e da Apps griglia in OGNI tema, cambia
    ordinamento/visibilità/posizione, riavvia e verifica che restino.

---

## 7. File coinvolti

| File | Ruolo |
|------|-------|
| `src/components/icons/*.h` | Data bitmap delle icone |
| `src/components/themes/BaseTheme.h` | `enum UIIcon` |
| `src/util/ShortcutRegistry.h` | `ShortcutDefinition` + helper ordine/visibilità |
| `src/CrossPointSettings.h` | Campi `...Shortcut`, `...ShortcutOrder`, `...ShortcutVisible` |
| `src/JsonSettingsIO.cpp` | Serializzazione JSON (load + save) |
| `src/components/themes/lyra/LyraTheme.cpp` | Mapping icone (24 + 32) |
| `src/components/themes/lyra/LyraCarouselTheme.cpp` | Mapping icone (24 + 32) |
| `src/components/themes/lyra/LyraMarcoand75Theme.cpp` | Mapping icone (32, app) |
| `src/components/themes/lyra/LyraCustomTheme.cpp` | Mapping icone (se presente) |

---

Documento di manutenzione del repository — aggiornalo quando cambia la struttura
delle icone o dei temi.
