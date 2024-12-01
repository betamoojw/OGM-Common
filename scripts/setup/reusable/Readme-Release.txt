Dieses Release kann unter Windows 10/11 folgendermaßen installiert werden:
- falls noch nicht geschehen: Das gesamte zip in ein Verzeichnis entpacken
- das Verzeichnis, in das entpackt wurde, öffnen
- rechte Maustaste auf Build-knxprod.ps1
- "Mit PowerShell ausführen" wählen, ggf. die Sicherheitswarnung mit "Datei öffnen" bestätigen
    (jetzt wird eine zum Release passende Produktdatenbank *.knxprod gebaut)

Es werden 2 Upload-Methoden unterstützt: 

USB-Upload (bei neuer Hardware steht nur diese Methode zur Verfügung):
    - Hardware an den USB-Port stecken (Hinweis: Es darf nur ein OpneKNX-Device am USB stecken),
    - rechte Maustaste auf "USB-Upload-Firmware-xxx.ps1"
    - "Mit PowerShell ausführen" wählen
        (jetzt wird die Firmware auf die Hardware geladen)
    - sobald die Firmware erfolgreich hochgeladen wurde, startet sich das Modul neu

KNX-Upload (Upload über den KNX-Bus, nur für Firmware-Update möglich):
    - Hardware muss am KNX-Bus angeschlossen sein und eine OpenKNX-Firmware muss bereits laufen (Firmwaere-Update ist beabsichtigt),
    - rechte Maustaste auf "KNX-Upload-Firmware-xxx.ps1"
    - "Mit PowerShell ausführen" wählen
        (jetzt wird die Firmware auf die Hardware geladen, dauert 10-20 Minuten!!!)
    - sobald die Firmware erfolgreich hochgeladen wurde, startet sich das Modul neu

Jetzt kann man die erzeugte knxprod in die ETS über den Katalog importieren und
danach wie gewohnt zuerst die Physikalische Adresse und nach der Parametrierung die Applikation programmieren.
Bitte noch die Applikationsbeschreibung beachten, dort stehen Hinweise zum update (ob man z.B. nur Firmware- oder nur ETS-Update braucht, normalerweise braucht man beides).
Fertig.
