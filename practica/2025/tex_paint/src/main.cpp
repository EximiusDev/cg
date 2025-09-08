#include <algorithm>
#include <stdexcept>
#include <vector>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "Model.hpp"
#include "Window.hpp"
#include "Callbacks.hpp"
#include "Debug.hpp"
#include "Shaders.hpp"

#define VERSION 20250901

Window main_window; // ventana principal: muestra el modelo en 3d sobre el que se pinta
Window aux_window; // ventana auxiliar que muestra la textura

void drawMain(); // dibuja el modelo "normalmente" para la ventana principal
void drawBack(); // dibuja el modelo con un shader alternativo para convertir coords de la ventana a coords de textura
void drawAux(); // dibuja la textura en la ventana auxiliar
void drawImGui(Window &window); // settings sub-window

float radius = 6; // radio del "pincel" con el que pintamos en la textura
glm::vec4 color = { 0.f, 0.f, 0.f, 1.f }; // color actual con el que se pinta en la textura
glm::vec2 p0 = { 0.f , 0.f };

Texture texture; // textura (compartida por ambas ventanas)
Image image; // imagen (para la textura, Image est� en RAM, Texture la env�a a GPU)

Model model_chookity; // el objeto a pintar, para renderizar en la ventan principal
Model model_aux; // un quad para cubrir la ventana auxiliar y mostrar la textura

Shader shader_main; // shader para el objeto principal (drawMain)
Shader shader_aux; // shader para la ventana auxiliar (drawTexture)

// callbacks del mouse y auxiliares para los callbacks
enum class MouseAction { None, ManipulateView, Draw };
MouseAction mouse_action = MouseAction::None; // qu� hacer en el callback del motion si el bot�n del mouse est� apretado
void mainMouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
void mainMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void auxMouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
void auxMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void dda(glm::vec2 p0, glm::vec2 p1);
void drawCircle(glm::vec2 p);
glm::vec2 getPointInCircle(float r, float theta, glm::vec2 center);
void brush_segment(glm::vec2 p0, glm::vec2 p1);
void blendPixel(int y, int x);


int main() {
	
	// main window (3D view)
	main_window = Window(800, 600, "Main View", true);
	glfwSetCursorPosCallback(main_window, mainMouseMoveCallback);
	glfwSetMouseButtonCallback(main_window, mainMouseButtonCallback);
	main_window.getCamera().model_angle = 2.5;
	
	glClearColor(1.f,1.f,1.f,1.f);
	shader_main = Shader("shaders/main");
	
	image = Image("models/chookity.png",true);
	texture = Texture(image);
	
	model_chookity = Model::loadSingle("models/chookity", Model::fNoTextures);
	
	// aux window (texture image)
	aux_window = Window(512,512, "Texture", true, main_window);
	glfwSetCursorPosCallback(aux_window, auxMouseMoveCallback);
	glfwSetMouseButtonCallback(aux_window, auxMouseButtonCallback);
	
	model_aux = Model::loadSingle("models/texquad", Model::fNoTextures);
	shader_aux = Shader("shaders/quad");
	
	// main loop
	do {
		glfwPollEvents();
		
		glfwMakeContextCurrent(main_window);
		drawMain();
		drawImGui(main_window);
		glFinish();
		glfwSwapBuffers(main_window);
		
		glfwMakeContextCurrent(aux_window);
		drawAux();
		drawImGui(aux_window);
		glFinish();
		glfwSwapBuffers(aux_window);
		
	} while( (not glfwWindowShouldClose(main_window)) and (not glfwWindowShouldClose(aux_window)) );
}


// ===== pasos del renderizado =====

void drawMain() {
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	
	texture.bind();
	shader_main.use();
	setMatrixes(main_window, shader_main);
	shader_main.setLight(glm::vec4{-1.f,1.f,4.f,1.f}, glm::vec3{1.f,1.f,1.f}, 0.35f);
	shader_main.setMaterial(model_chookity.material);
	shader_main.setBuffers(model_chookity.buffers);
	model_chookity.buffers.draw();
}

void drawAux() {
	glDisable(GL_DEPTH_TEST);
	texture.bind();
	shader_aux.use();
	shader_aux.setMatrixes(glm::mat4{1.f}, glm::mat4{1.f}, glm::mat4{1.f});
	shader_aux.setBuffers(model_aux.buffers);
	model_aux.buffers.draw();
}

void drawBack() {
	glfwMakeContextCurrent(main_window);
	glDisable(GL_MULTISAMPLE);

	/// @ToDo: Parte 2: renderizar el modelo en 3d con un nuevo shader de forma 
	///                 que queden las coordenadas de textura de cada fragmento
	///                 en el back-buffer de color
	
	glEnable(GL_MULTISAMPLE);
	glFlush();
	glFinish();
}

void drawImGui(Window &window) {
	if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) return;
	// settings sub-window
	window.ImGuiDialog("Settings",[&](){
		ImGui::SliderFloat("Radius",&radius,1,50);
		ImGui::ColorEdit4("Color",&(color[0]),0);
		
		static std::vector<std::pair<const char *, ImVec4>> pallete = { // colores predefindos
			{"white" , {1.f,1.f,1.f,1.f}},
			{"pink"  , {0.749f,0.49f,0.498f,1.f}},
			{"yellow", {0.965f,0.729f,0.106f,1.f}},
			{"black" , {0.f,0.f,0.f,1.f}} };
		
		ImGui::Text("Pallete:");
		for (auto &p : pallete) {
			ImGui::SameLine();
			if (ImGui::ColorButton(p.first, p.second))
				color[0] = p.second.x, color[1] = p.second.y, color[2] = p.second.z;
		}
		
		if (ImGui::Button("Reload Image")) {
			image = Image("models/chookity.png",true);
			texture.update(image);
		}
	});
}



// ===== callbacks de la ventana auxiliar (textura) =====

void auxMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (ImGui::GetIO().WantCaptureMouse) return;
	if (action==GLFW_PRESS) {
		mouse_action = MouseAction::Draw;
		
		/// @ToDo: Parte 1: pintar un punto de radio "radius" en la imagen
		///                 "image" que se usa como textura
		double xpos, ypos;
        //getting cursor position
        glfwGetCursorPos(window, &xpos, &ypos);
		

		int  win_width, win_height; 
		glfwGetWindowSize(window,&win_width, &win_height);

		// Image info
		auto img_width = image.GetWidth();
		auto img_height = image.GetHeight();
		
		// S and T coordinates convertion
		auto w_s = (float)(xpos/win_width);
		auto w_t = 1-(float)(ypos/win_height);
		
		//std::cout << "windows (s,t): " << w_s << " ," << w_t << "; \n";
		
		// Convert into image coordinates
		auto i_xpos = w_s*img_width;
		auto i_ypos = w_t*img_height;

		p0 = glm::vec2(i_xpos,i_ypos);

	} else {
		mouse_action = MouseAction::None;
	}
}

void auxMouseMoveCallback(GLFWwindow* window, double xpos, double ypos) {
	if (mouse_action!=MouseAction::Draw) return;
	
	/// @ToDo: Parte 1: pintar un segmento de ancho "2*radius" en la imagen
	///                 "image" que se usa como textura

	// Windows info
	int  win_width, win_height; 
	glfwGetWindowSize(window,&win_width, &win_height);
	// xpos and ypos are the cursor position inside the window.

	// Image info
	auto img_width = image.GetWidth();
	auto img_height = image.GetHeight();
	
	// S and T coordinates convertion
	auto w_s = (float)(xpos/win_width);
	auto w_t = 1-(float)(ypos/win_height);
	
	//std::cout << "windows (s,t): " << w_s << " ," << w_t << "; \n";
	
	// Convert into image coordinates
	auto i_xpos = w_s*img_width;
	auto i_ypos = w_t*img_height;
	auto p1 = glm::vec2(i_xpos,i_ypos);
	brush_segment(p0,p1);
	p0=p1;
	texture.update(image);
}
// Algoritmo DDA para trazar una línea en la textura
void dda(glm::vec2 p0, glm::vec2 p1)
{
    float dx = p1.x - p0.x;
    float dy = p1.y - p0.y;

    if (dx == 0 && dy == 0) return;

    if (fabs(dx) > fabs(dy)) {
        // caso x-dominante
        if (p0.x > p1.x) std::swap(p0, p1);

        float slope = dy / dx;
        float y = p0.y;
        for (int x = (int)p0.x; x <= (int)p1.x; x++) {
            blendPixel((int)round(y),x);
            y += slope;
        }
    } else {
        // caso y-dominante
        if (p0.y > p1.y) std::swap(p0, p1);

        float slope = dx / dy;
        float x = p0.x;
        for (int y = (int)p0.y; y <= (int)p1.y; y++) {
            blendPixel(y,(int)round(x));
            x += slope;
        }
    }
}

void blendPixel(int y, int x) {
        glm::vec3 current_color = image.GetRGB(y, x); 
        float alpha = color.w;

        glm::vec3 src = glm::vec3(color.x, color.y, color.z);
        glm::vec3 dst = current_color;

        glm::vec3 out = alpha * src + (1.0f - alpha) * dst;
        image.SetRGB(y,x, out);
    };

// Pincel: dibuja un segmento "grueso" pintando círculos a lo largo de la línea
void brush_segment(glm::vec2 p0, glm::vec2 p1)
{
	float dx = p1.x - p0.x;
	float dy = p1.y - p0.y;
	
	if (dx == 0 && dy == 0) return; // un solo punto
	
	if (fabs(dx) > fabs(dy)) {
		// caso x-dominante
		if (p0.x > p1.x) std::swap(p0, p1);
		
		float slope = dy / dx;
		float y = p0.y;
		for (int x = (int)p0.x; x <= (int)p1.x; x++) {
			drawCircle(glm::vec2(x, (int)round(y)));
			y += slope;
		}
	} else {
		// caso y-dominante
		if (p0.y > p1.y) std::swap(p0, p1);
		
		float slope = dx / dy;
		float x = p0.x;
		for (int y = (int)p0.y; y <= (int)p1.y; y++) {
			drawCircle(glm::vec2((int)round(x), y)); 
			x += slope;
		}
	}
}

void drawCircle(glm::vec2 p) {
    const double pi = 3.14159265359;

    for (int r = 0; r < radius; r++) {
        int samples = 2 * r + 1; 
        double d_theta = 2 * pi / samples;

        double theta = 0;
        while (theta < 2 * pi) {
            glm::vec2 p_0 = getPointInCircle(r, theta, p);
            glm::vec2 p_1 = getPointInCircle(r, theta + d_theta, p); 
            dda(p_0, p_1);
            theta += d_theta;  // avanzar!
        }
    }
}

glm::vec2 getPointInCircle(float r, float theta, glm::vec2 center) {
    return glm::vec2(center.x + r * cos(theta), center.y + r * sin(theta));
}



// ===== callbacks de la ventana principal (vista 3D) =====

void mainMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (ImGui::GetIO().WantCaptureMouse) return;
	if (action==GLFW_PRESS) {
		if (mods!=0 or button==GLFW_MOUSE_BUTTON_RIGHT) {
			mouse_action = MouseAction::ManipulateView;
			common_callbacks::mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, action, mods);
			return;
		}
		
		mouse_action = MouseAction::Draw;
		
		/// @ToDo: Parte 2: pintar un punto de radio "radius" en la imagen
		///                 "image" que se usa como textura
		
	} else {
		if (mouse_action==MouseAction::ManipulateView)
			common_callbacks::mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, action, mods);
		mouse_action = MouseAction::None;
	}
}

void mainMouseMoveCallback(GLFWwindow* window, double xpos, double ypos) {
	if (mouse_action!=MouseAction::Draw) {
		if (mouse_action==MouseAction::ManipulateView);
			common_callbacks::mouseMoveCallback(window,xpos,ypos);
		return; 
	}
	
	/// @ToDo: Parte 2: pintar un segmento de ancho "2*radius" en la imagen
	///                 "image" que se usa como textura
	
}
