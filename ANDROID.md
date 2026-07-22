# OpenGothic — Android port (экспериментальный)

## Что это

Порт OpenGothic на Android (arm64, Vulkan, NativeActivity). Движок собирается в `libGothic2Notr.so`,
игровые данные НЕ входят в APK — нужна лицензионная копия Gothic 2: Ночь Ворона (или Gothic 1).

## Как собрать APK (через GitHub Actions)

1. Создай новый репозиторий на github.com (например `opengothic-android`), можно приватный.
2. В терминале мака:

   ```bash
   cd ~/Downloads/opengothic-port/OpenGothic   # распакованная папка с этим кодом
   git init -b main
   git add -A
   git commit -m "OpenGothic Android port"
   git remote add origin https://github.com/ТВОЙ_ЛОГИН/opengothic-android.git
   git push -u origin main
   ```

3. На GitHub открой вкладку **Actions** → workflow **Android** запустится сам после пуша.
4. Когда сборка позеленеет, внизу страницы запуска будет артефакт **OpenGothic-Android** — внутри `app-release.apk`.

## Установка на телефон

1. Скачай и установи APK (разреши установку из неизвестных источников).
2. Скопируй папку с установленной Windows-версией игры на телефон в `/sdcard/Gothic`, чтобы было:
   - `/sdcard/Gothic/Data/*.vdf`
   - `/sdcard/Gothic/_work/...`
3. Запусти OpenGothic — приложение попросит доступ «All files access», выдай и вернись.

## Логи (если что-то падает)

```bash
adb logcat -s OpenGothic Gothic2Notr AndroidRuntime DEBUG threaded_app
```

или просто `adb logcat | grep -i gothic`. Присылай вывод — по нему чинится всё.

## Управление (v1)

- Тач = мышь (интерфейс движка уже имеет мобильные ветки, как в iOS-порте).
- Кнопка/жест «Назад» = Esc (игровое меню).

## Известные ограничения v1

- Сворачивание приложения может приводить к перезапуску игры.
- Производительность и совместимость шейдеров с Adreno — предмет итераций.
