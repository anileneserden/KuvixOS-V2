# KEF SDK v1 Draft

Bu dalin amaci, KEF uygulama gelistiricisinin gorecegi C++ yuzeyini netlestirmektir.
Bu dokuman runtime implementasyonu degil, SDK sozlesmesidir.

## Hedef

Gelistirici su sekilde uygulama yazabilmelidir:

```cpp
#include <sdk/kef/prelude.hpp>

class HelloApp : public Console {
public:
    int Main() override {
        Print("Hello World\n");
        return 0;
    }
};

KEF_APP(HelloApp)
```

veya:

```cpp
class DemoApp : public Window {
public:
    void OnDraw() override {
        DrawText(20, 20, "Hello Window");
    }
};

KEF_APP(DemoApp)
```

## Uygulama Tipleri

- `kef::Console`
  Terminal/console tabanli uygulamalar icin kullanilir.
- `kef::Window`
  Pencere tabanli uygulamalar icin kullanilir.

## Ortak Yasam Dongusu

Tum uygulamalar `kef::App` taban sinifindan turemelidir.

Desteklenecek ortak callback'ler:

- `OnCreate()`
- `OnDestroy()`
- `OnUpdate()`

## Console API

`kef::Console` asagidaki davranislari saglamalidir:

- `int Main()` ana giris noktasi
- `Print(const char* text)` terminale yazar
- `PrintLine(const char* text)` satir sonu ile yazar
- `Exit(int code = 0)` uygulamayi sonlandirir
- `ArgCount()` ve `ArgAt(int index)` ile arguman erisimi

## Window API

`kef::Window` asagidaki davranislari saglamalidir:

- `OnDraw()`
- `OnKey(int key)`
- `OnMouse(int x, int y, int buttons)`
- `DrawText(int x, int y, const char* text)`
- `Close()`
- `Invalidate()`

## Varsayilan Pencere Degerleri

Bir pencere uygulamasi width/height/title belirtmezse SDK tarafi su degerleri kullanir:

- width: `640`
- height: `480`
- title: `"Kef Window"`

Bu degerler constructor veya metadata override ile degistirilebilir.

## Registration Modeli

Gelistirici dogrudan `main()` yazmaz. Bunun yerine bir app sinifi tanimlar ve sunu kullanir:

```cpp
KEF_APP(MyApp)
```

Bu macro ileride su isleri yapacaktir:

- app turunu metadata'ya yazmak
- varsayilan pencere bilgilerini kaydetmek
- runtime'in bulacagi giris/factory bilgisini saglamak

## KEF Metadata Eslemesi

SDK tarafi asagidaki alanlari KEF metadata'ya indirmelidir:

- app kind: `console` veya `window`
- default width
- default height
- default title
- app class entry/factory bilgisi
- registration tarafinda manifest uretimi

## Ornekler

Console ornegi:

```cpp
#include <sdk/kef/prelude.hpp>

class HelloConsole : public Console {
public:
  int Main() override {
    Print("Hello World from Console app\n");
    return 0;
  }
};

KEF_APP(HelloConsole)
```

Window ornegi:

```cpp
#include <sdk/kef/prelude.hpp>

class HelloWindow : public Window {
public:
  HelloWindow() : Window(800, 480, "Hello Window") {}

  void OnDraw() override {
    DrawText(24, 24, "Hello Window from KEF SDK");
  }
};

KEF_APP(HelloWindow)
```

## Bu Dalin Siniri

Bu dalda hedef su degildir:

- gercek window host implementasyonu
- network API implementasyonu
- file API implementasyonu
- tam C++ compiler backend

Bu dalda hedef su dur:

- SDK yuzeyini tasarlamak
- sinif hiyerarsisini netlestirmek
- metadata modelini tanimlamak
- sonraki host dallari icin ortak sozlesmeyi sabitlemek