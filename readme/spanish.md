# Hux

Hux es una WYSIAWYG (del ingl�s "What You See Is What You Get", que significa "lo que se ve es lo que se obtiene") editor de script de terminal para [Aleph One](https://alephone.lhowon.org/), desarrollado usando [Qt](https://www.qt.io/). La GUI permite a los usuarios crear, editar, previsualizar cualquier escenario compatible con AO (AlephOne), con controles para la alineaci�n de im�genes y texto, forma, etc.

Todas las operaciones necesarias pueden realizarse atreves de la herramienta, minimizando el riesgo causado por una sintaxis incorrecta y reduciendo el costo de tiempo para la creaci�n y edici�n de terminales.

La app fue desarrollada usando Qt versi�n 5.12.9, pero deber�a ser compatible con las versiones m�s nuevas.

_NOTA: Hux proporciona una representaci�n cercana, pero no 1:1 de la representaci�n de la terminal de Aleph One. Se conserva todo el contenido del scripteado, pero puede haber peque�as diferencias en el resultado mostrado (por ejemplo, ajustes de l�neas)_

## Compilaci�n desde el c�digo fuente

Hux utiliza [CMake](https://cmake.org/) como sistema de compilaci�n multiplataforma. La versi�n m�nima requerida es la 3.16.

Para mayor comodidad, el proyecto tambi�n incluye un archivo [CMakePresets.json](https://learn.microsoft.com/en-us/cpp/build/cmake-presets-vs) con algunas configuraciones b�sicas de compilaci�n para Windows y Linux. Estas configuraciones pueden ajustarse a�n m�s mediante un archivo CMakeUserPresets.json en el entorno local del usuario.

### Compilando en Windows

La opci�n m�s sencilla es compilar usando Visual Studio 2022 o Visual Studio Code con las extensiones correspondientes, ya que ambos pueden utilizar los presets de CMake.

*NOTA: el comando de CMake `find_package` actualmente no funciona correctamente con Qt en Windows, por lo que debes establecer la variable `CMAKE_PREFIX_PATH` al directorio donde est� instalado Qt6.*

### Copilando en Linux

Configura los requisitos para Linux/X11 seg�n la [Qt documentation](https://doc.qt.io/qt-6/linux.html), luego instala el paquete "Qt6 base" (por ejemplo, `sudo apt install qt6-base-dev`), que incluye las caracter�sticas necesarias de Qt. Instala CMake, luego configura y compila.

*NOTA: aseg�rate de establecer un directorio de salida (ej. `CMAKE_RUNTIME_OUTPUT_DIRECTORY`), de lo contrario el nombre de la aplicaci�n podr�a colisionar con alguna de las carpetas creadas por CMake durante la compilaci�n.*

### Compilando para Mac

TODO

## Empezando

Una vez que abra Hux, primero debes [importar](#importing-scenarios) un escenario desde la carpeta dividida (split folder), or [abrir](#loading-scenarios) un archivo de escenario Hux.

### Importando escenarios

Hux puede hacer uso de carpetas divididas generadas por [Atque](https://sourceforge.net/projects/igniferroque/) para cargar la informaci�n de la terminal.

Para importar un escenario desde una carpeta dividida (split folder), simpemente haga click en _File -> Import Scernacio Scrips_ y seleccione el directorio ra�z de un escenario dividido generado por Atque. El navegador de escenario mostrar� en pantalla todos los niveles que tengan un archivo script v�lido.

 Cuando comienza a trabajar en un nuevo escenario, o desea editar un escenario que no tiene datos espec�ficos de Hux, debe crear una carpeta dividida (split folder) e importarla, despu�s de esto puede [agregar niveles](#editing-scenarios) que requieran datos de terminal.

*NOTA: �Aseg�rese de que la carpeta dividida tambi�n tenga una carpeta "Resources" (Recursos) v�lida!*

### Cargando escenarios

Hux usa un archivo personalizado para guardar/cargar script de terminal (usando JSON). 
Este archivo es leido y escrito por la aplicaci�n, no es necesaria una edici�n manual (excepto cuando se combinan cambios, ejemplo v�a herramientas de diferencias)

Para cargar un archivo de escenario Hux, click _File -> Load Scenario_ y seleccione un archivo JSON v�lido de escenario.

El archivo personalizado se recomienda para los desarrolladores que trabajan en terminales, ya que les permite almacenar metadatos adicionales y puede fusionar m�s f�cilmente los cambios entre varios usuarios.

*NOTA: Hux espera que los archivos del escenario est�n en el mismo directorio que la carpeta "Resources" apra el mismo escenario; de lo contrario, no podr� cargar las im�genes a las que se hace referencia en los scripts.*

### Ventana principal

Esta es la ventana principal que se muestra por primera vez cuando se carga la aplicaci�n.

El lado izquierdo de la ventana muestra el [Navegador de Escenarios](#scenario-browser) y una tabla de informaci�n para la terminal actualmente seleccionada.

El lado derecho muestra un [Navegador de Pantalla](#screen-browser) para el terminal seleccionado, una tabla que contiene informaci�n sobre la pantalla seleccionada actualmente y una vista previa de la terminal.

#### Navegador de Escenarios

Cada nivel en el escenario est� representado por una carpeta en la vista de lista.

Hacer doble click en un nivel, aparecer� una lista de las terminales contenidos en dicho nivel.

Al hacer clic en un termina, se mostrar� su contenido en el [Explorador de Pantalla](#screen-browser). Para volver a la vista principal del escenario, utilice el bot�n "Up" (Arriba) sobre el *Explorador de Escenarios*

#### Explorador de Pantallas

Las pantallas de cada terminal se dividen en las secciones "UNFINISHED" (Sin terminar) y "FINISHED" (Terminada)

Al hacer clic en cualquier elemento de la pantalla, se mostrar� su informaci�n y aparecer� una vista previa en la pantalla.

Lo botones debajo de la pantalla tambi�n se pueden usar para navegar entre las pantalla del terminal seleccionado.

### Editando escenarios

El [Explorado de Escenarios](#scenario-browser) te permite editar el contenido del escenario, incluidos los niveles y sus terminales.

- Agregar y eliminar se realiza a trav�s de los botones debajo de la vista [Navegador de Escenarios](#scenario-browser).
- Los niveles se puede editar haciendo click derecho y seleccionando "Edit Level" (Editar Nivel) [Editor de Nivel](#level-editor).
- Puede arrastrar y soltar para reordenar niveles y terminales. Cambiar el orden de los niveles no afercta a la exportaci�n, es solo para conveniencia del usuario.
  - *NOTA: cambiando el orden de las terminales cambiar� el ID  de los terminales en el script exportado. Esto puede invalidad las referencias en los datos del mapa!*
- al hacer doble click en un nivel, se abre y muestra una lista de sus terminales.
- Las terminales se puede copiar y pegar dentro y entre los niveles. Para copiar los terminales seleccionados, haga click derecho y selecciones "Copy", y luego haga click derecho nuevamente en la ubicaci�n deseada y selecciones "Paste".
- Al hacer doble click en un terminal en el [Navegador de Escenarios](#scenario-browser) abre una ventana del [Editor de Terminales](#terminal-editor), que permite a los usuarios editar el contenido de la terminal.

### Editor de Nivel

Esta ventana permite modificar los atributos del nivel. Puede editar el nombre del nivel, el nombre del archivo de script y el nombre de la carpeta de nivel.

El nombre del nivel es solo para comodidad del usuario. Su nombre para mostrar en el juego debe configurarse en los scripts relevantes fuera de Hux.

Al agregar un nuevo nivel o reorganizar el escenario, debe proporcionar la carpeta y los nombres de los script correctos. Esto le permite a Hux sobrescribir los archivos apropiados al exporta el escenario.

*NOTA: �la implementaci�n actual excluye caracteres de carpetas y nombres de archivos que no est�n permitidos en el sistema de archivos de Windows!*

### Editor de Terminal

Cuando hace click en una terminal en el [Navegador de Escenario](#scenario-browser), se abre una ventana del [Editor de Terminal](#terminal-editor) para este terminal.

El lado izquierda de la ventana muestra los controles para editar los atributos de la terminal (e.j. informaci�n de teletransportacion) y el [Explorador de Pantalla](#screen-browser), que se puede usar para ver y modificar las pantalla dentro de la terminal. 

El lado derecho contiene una vista de editor donde se puede editar los contenidos de la pantalla y los metadatos, junto con un vista previa de la pantalla editada actualmente.

- Puede dar a los terminales un nombre personalizado que para identificarlos f�cilmente. Estos datos son espec�ficos de Hux, no se exportan con los archivos de script del terminal.
- El [Explorador de Pantallas](#screen-browser) contienen dos carpetas, correspondiente a los grupos de pantalla "UNFINISHE" y "FINISHED". Haga doble click en una carpeta para ver y editar su contenido.
- La edici�n de la lista de pantallas es similar a la edici�n de niveles ene el [Navegador de Escenarios](#scenario-browser):
  - Agregar o eliminar pantalla usando los botones debajo den navegador.
  - Mover pantallas usando arrastrar y soltar.
  - Copiar y pegar pantallas seleccionadas click derecho y seleccionando la acci�n apropiada.
- Para editar una pantalla, selecci�nela en el [Screen Browser](#screen-browser). Esto actualizar� la vista del editor de pantalla y la vista previa. AL cambiar de pantalla, se guardar�n los cambios en la �ltima pantalla seleccionada.
- Una vez que haya terminado de editar, puede cerrar la ventana usando "OK". Esto aplicara los cambiaos al terminal en el [Navegador de Escenario](#scenario-browser).

### Pantalla de edici�n

Cuando se selecciona una pantalla en el [Explorador de Pantalla](#screen-browser), la vista del editor se actualizar� con su contenido y la vista previa de la pantalla se mostrar� en la pantalla.

Cualquier cambio realizado en el contenido de la pantalla actualizar� autom�ticamente la vista previa.

La p�gina tambi�n muestra n�meros de l�nea para ayudar a realizar un seguimiento de qu� tan cerca est� el contenido del texto del l�mite de l�nea.

Si el contenido de texto de una pantalla excede el l�mite de l�nea para una sola p�gina, la pantalla de vista previa mostrar� un contador de p�gina. El valor del contador corresponde al n�mero (estimado) de p�ginas en las que el motor dividir� el texto.

- Las vistas previas para el salto de p�gina aun no est�n implementadas. Se recomienda que no permia que el texto exceda el l�mite de l�nea, ya que esto permite un control menos preciso sobre el dise�o del texto.

- Para obtener m�s detalles sobre la sintaxis y la l�gica de secuencias de comandos del terminal, consulte el manual de Marathon Infinity.

### Grabando escenarios

Si tiene cambios sin guardar, puede guardar el archivo de escenario utilizando _File -> Export Scenario Scripts_, Esto guardar� los datos de la secuencia de comandos del terminal en el archivo JSON espec�fico de Hux.

Al guardar por primera vez, se le pedir� un nombre y una ubicaci�n para el escenario. Los guardados subsiguientes sobrescribir�n este archivo. Para guardar el escenario en una ubicaci�n diferente, puede usar _File->Save Scenario As_.

*NOTA: �los datos espec�ficos del editor (por ejemplo, los nombres de terminales) solo se guardan en el archivo JSON. �Estos datos se pierden si el usuarios intenta volver a cargar un escenarios desde los archivos .txt exportados!*

Tambi�n se le puede solicitar al usuario que guarde al salir (esto omitir� los pasos anteriores).

*NOTA: la aplicaci�n espera que el archivo del escenario est� en la misma carpeta que la carpeta de Resources (Recursos) para el escenario; de lo contrario, �no podr� acceder a las im�genes a las que se hace referencia!*

### Exportando escenarios

Para probar los cambios de la terminal en Aleph One, los scripts de terminal deben exportarse a una carpeta dividida y luego fusionarse con Atque.

Para exportar un escenario, haga click en _File -> Export Scenario Scripts_, que abre un cuadro de di�logo de exportaci�n. Esto mostrar� una lista de todos los niveles y le permitir� al usuario obtener una vista previa del contenido del script. Al hacer click en un nivel de la lista, se mostrar� el script de terminal Aleph One generado.

Al hacer click en OK, se le pedir� al usuario que seleccione una carpeta de destino. Esta puede ser una carpeta dividida (split folder) existente, en cuyo caso Hux sobrescribir� cualquier script de terminal existente. Si no se encuentra un script coincidente para un nivel, Hux creara una nueva carpeta y un archivo sript.

*NOTA: �aseg�rese de ajustar los atributos de nivel para que Hux sobrescriba los archivos correctos en las carpetas correctas!*

## Licencia

Vea el archivo de [LICENCIA](https://github.com/janos-ijgyarto/HuxQt/blob/master/LICENSE).

## Links �tiles

- PORHACER: un enlace al manual correspondiente para secuencia de comandos de terminal AO (por ejemplo, el manual de Infinity)