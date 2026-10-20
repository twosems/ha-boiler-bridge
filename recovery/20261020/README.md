# Приватные оригиналы прошивок через.16

`boiler-originals-through-16.tar.xz.encrypted` содержит20 OTA/factory образов десяти найденных версий (.4-office,.7,.8,.9,.10,.12,.13,.14,.15,.16), исходныеprivate настройки и найденныеoriginalsource архивы. Формат BOILER-HYBRID-v1, AES256GCM сRSAOAEP-SHA256; используется существующий publickey recovery/boiler-recovery-public-20261012.pem. Закрытый ключ boiler-recovery-private-20261012.pem ранее передан пользователю отдельно и НЕ содержится вGitHub.

Команда восстановления вприватномокружении:

```sh
python tools/decrypt_backup.py recovery/20261020/boiler-originals-through-16.tar.xz.encrypted /private/boiler-recovery-private-20261012.pem /private/boiler-originals-through-16.tar.xz
```

Подкаталог version6 содержит отдельный исходныйZIPпакет.6 (OTA/factory), также зашифрованный; соседнийarchive.json задаёт егоhash. Применить тотжеdecrypt_backup.py, указав путь кзашифрованномуZIP и выход .zip. Реальные данные не помещать вcheckout. AESGCMroundtrip проверен до записи; проверка расшифровки пользовательскимRSAprivatekey не выполнялась, самключ нечитался.

`original-files.json` задаёт размеры/hash каждогооригинала. .15образы взяты изпроверенногоrollback15, анеперезаписанногокэша. Внимание: часть исходныхконтрактов/manifest содержитстарыеNOT_INSTALLEDстатусы; это историческиеоригиналы. Актуальная установкаописана вCURRENT/DEPLOYMENT-20261020-16.

`work-recovery.git.bundle` сохраняетточную восстановленнуюлокальнуюGitисторию.13–.16 сисходнымHEAD a5fcb42. Она неимеетобщейancestryсupstream; использоватьдляанализа/восстановления, анеforce-push. Пример: `git clone recovery/20261020/work-recovery.git.bundle /private/work-recovery`.

Не найден самостоятельныйснимок.11 и оригиналы.5 в проверенныхместах. Не восстанавливать бинарники попереименованиюдругойверсии.
