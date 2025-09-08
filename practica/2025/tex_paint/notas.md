Parte 1:

* Implementar los algoritmos de rasterización necesarios para poder dibujar sobre la textura:
  * Al mover el ratón con el botón izquierdo apretado se debe generar una curva contigua
    * Se debe poder controlar el ancho de la curva
    * Se debe tener en cuenta el canal *alpha* del color seleccionado para controlar la opacidad con que se pinta
* Ayudas:
  * para resolver lo del ancho de la curva, pensar en pintar puntos gordos y no preocuparse por la eficiencia del repintado
  * como obtener las coords desde glfwf... como el tamaño de la ventana o de la imagen...
  * como escribir en la imagen
    * ojo, no pinte fuera de la imagen (tal vez no ayudar, ya hay un assert)
  * llamar al update de la textura!
* Extras:
  * estilo aerosol?