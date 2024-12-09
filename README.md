Mensajito para ivan (si lo ves mañana). 
Como te habia dicho estan hechos en teoria el umask y el C^. 
El umask lo probe 500 veces, no creo que haya algun punto que de problema.
El ^C (sigint) en teoria bajo control. Quiza hace falta probarlo con casos particulares. pero en papel esta listo.
El ^Z (sigtstp) tengo el manejador como un boceto. No esta pensado para que funcione aun. (No le des vueltas todavia lol)

Te puedes rayar con configurar_senales(). Es literalmente la unica manera que consegui de que, haciendo ^Z y parando un proceso, el programa supiese volver al bucle principal,
de todas las demas maneras, se queda atrapado y no deja ejecutar mas nada.

Minetras logro que la funcionalidad vaya bien, creo que es un problema que podemos posponer. Peor de los caos aprendemos como funciona (skull emoji)
sigaction() es supuestamente la version moderna del signal() que vemos en clase.

De resto vemos mañana, que ya es tardecillo.
Cualquier cosa me dices.
Arturo.
