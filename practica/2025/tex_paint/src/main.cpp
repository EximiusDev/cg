#include <algorithm>
#include <stdexcept>
#include <vector>
#include <string>
#include <map>
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

float radius = 5; // radio del "pincel" con el que pintamos en la textura
glm::vec4 color = { 0.f, 0.f, 0.f, 1.f }; // color actual con el que se pinta en la textura

Texture texture; // textura (compartida por ambas ventanas)
Image image; // imagen (para la textura, Image est� en RAM, Texture la env�a a GPU)

Model model_chookity; // el objeto a pintar, para renderizar en la ventan principal
Model model_aux; // un quad para cubrir la ventana auxiliar y mostrar la textura

Shader shader_main; // shader para el objeto principal (drawMain)
Shader shader_aux; // shader para la ventana auxiliar (drawTexture)



// CUSTOM VARS
glm::vec2 p0_2d = {0.f, 0.f};
glm::vec2 p1_2d = {0.f, 0.f};
std::map<glm::vec3,glm::vec2> colorToImage;
std::map<glm::vec2,glm::vec3> imageToColor;
Image image_colormap;
Shader shader_flat; // shader plano

// CUSTOM FUNCTIONS
void drawCircle(int radius,glm::vec2 point);
void dda(glm::vec2 p_0,glm::vec2 p_1,std::string type="line");
void blendPixel(int y, int x);
void makeColorMap();
void printColorToImage();
void printImageToColor();

// Comparadores para usar glm::vec como clave en std::map
namespace glm {
    inline bool operator<(const glm::vec2& a, const glm::vec2& b) {
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y;
    }

    inline bool operator<(const glm::vec3& a, const glm::vec3& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
}



// callbacks del mouse y auxiliares para los callbacks 
enum class MouseAction { None, ManipulateView, Draw };
MouseAction mouse_action = MouseAction::None; // qu� hacer en el callback del motion si el bot�n del mouse est� apretado
void mainMouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
void mainMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void auxMouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
void auxMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

int main() {
	
	// main window (3D view)
	main_window = Window(800, 600, "Main View", true);
	glfwSetCursorPosCallback(main_window, mainMouseMoveCallback);
	glfwSetMouseButtonCallback(main_window, mainMouseButtonCallback);
	main_window.getCamera().model_angle = 2.5;
	
	glClearColor(1.f,1.f,1.f,1.f);
	shader_main = Shader("shaders/main");
	shader_flat = Shader("shaders/flat");
	
	image = Image("models/chookity.png",true);
	
	// CUSTOM
	image_colormap = Image("models/chookity.png",true);
	makeColorMap();

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
    // glDisable(GL_MULTISAMPLE);

	/// @ToDo: Parte 2: renderizar el modelo en 3d con un nuevo shader de forma 
	///                 que queden las coordenadas de textura de cada fragmento
	///                 en el back-buffer de color

	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glDisable(GL_MULTISAMPLE); // evita promediar subpixeles
	glDisable(GL_BLEND);       // evita combinar colores con transparencia
	glDisable(GL_DITHER);      // evita correcciones de dithering
	
	texture.bind();
	shader_flat.use();
	setMatrixes(main_window, shader_flat);
	shader_flat.setLight(glm::vec4{-1.f,1.f,4.f,1.f}, glm::vec3{1.f,1.f,1.f}, 0.35f);
	shader_flat.setMaterial(model_chookity.material);
	// Pass texture size so the shader can encode pixel coordinates into color
	shader_flat.setUniform("texSize", glm::vec2((float)image.GetWidth(), (float)image.GetHeight()));
	shader_flat.setBuffers(model_chookity.buffers);
	model_chookity.buffers.draw();

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
		
		double x,y;
		glfwGetCursorPos(window,&x,&y);
		
		p0_2d = glm::vec2(x,y);

		// Adaptar a coordenadas S y T
		int  win_width, win_height;
    	glfwGetWindowSize(window,&win_width, &win_height);
		auto p0_st = glm::vec2(p0_2d.x / win_width, 1 - (p0_2d.y/win_height)); 
		auto p0_img = glm::vec2((float)image.GetWidth()*p0_st.x,(float)image.GetHeight()*p0_st.y); 

		drawCircle(radius,p0_img);
		texture.update(image);
	} else {
		mouse_action = MouseAction::None;
	}
}

void auxMouseMoveCallback(GLFWwindow* window, double xpos, double ypos) {
	if (mouse_action!=MouseAction::Draw) return;
	
	/// @ToDo: Parte 1: pintar un segmento de ancho "2*radius" en la imagen
	///                 "image" que se usa como textura
	
	p1_2d = glm::vec2(xpos,ypos);

	int  win_width, win_height;
    glfwGetWindowSize(window,&win_width, &win_height);
	auto p1_st = glm::vec2(p1_2d.x / win_width, 1 - (p1_2d.y/win_height)); 
	auto p0_st = glm::vec2(p0_2d.x / win_width, 1 - (p0_2d.y/win_height)); 
	auto p0_img = glm::vec2((float)image.GetWidth()*p0_st.x,(float)image.GetHeight()*p0_st.y); 
	auto p1_img = glm::vec2((float)image.GetWidth()*p1_st.x,(float)image.GetHeight()*p1_st.y);
	dda(p0_img,p1_img,"stroke");
	p0_2d = p1_2d;
	texture.update(image);
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

		if (button==GLFW_MOUSE_BUTTON_LEFT) {
			mouse_action = MouseAction::Draw;
			
			// Ponemos el mapa de colores
			//texture.update(image_colormap);

			drawBack();
			glFinish();
			
			double wx,wy;
			glfwGetCursorPos(window,&wx,&wy);

			int  wwidth, wheight;
    		glfwGetWindowSize(window,&wwidth, &wheight);

			int px = (int)(wx);
			int py = (int)(wheight - wy);

			p0_2d = glm::vec2(px,py);
			
			float zbf ;
			glReadBuffer(GL_DEPTH);
			glReadPixels(px,py,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&zbf);

			std::cout << "P: " << px << ", " << py << std::endl;			
			std::cout << "Z_VALUE: " << zbf << std::endl;
			
			if(zbf < 1.f) // Si no es el Z_FAR
			{	
				std::cout << "zbf function triggered" << std::endl;

				// Leemos el color del pixel
				glReadBuffer(GL_BACK);
				unsigned char color_value[3];
				glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, color_value);

				// Decode the packed 24-bit index from the read-back RGB
				int r = (int)color_value[0];
				int g = (int)color_value[1];
				int b = (int)color_value[2];

				int idx = (r << 16) | (g << 8) | b;
				int texW = image.GetWidth();
				int texH = image.GetHeight();
				int px_tex = idx % texW;
				int py_tex = idx / texW;

				glm::vec2 p0_tex = glm::vec2((float)px_tex, (float)py_tex);

				std::cout << "READ COLOR (packed idx): " << idx << " -> POS: "
						  << px_tex << ", " << py_tex << std::endl;
				
				
				drawCircle(radius, p0_tex);
				
				p0_2d = p0_tex;
				texture.update(image);
			}
		}
		
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
	
	
	drawBack();
	glFinish();
	
	int  wwidth, wheight;
	glfwGetWindowSize(window,&wwidth, &wheight);
	
	int px = (int)(xpos);
	int py = (int)(wheight-ypos);
	
	p1_2d = glm::vec2(px,py);
	
	//p0_2d = glm::vec2(px,py);
	
	float zbf ;
	glReadBuffer(GL_DEPTH);
	glReadPixels(px,py,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&zbf);
	
	if(zbf < 1.f) // Si no es el Z_FAR
	{	
		std::cout << "zbf function triggered" << std::endl;
		
		// Leemos el color del pixel
		glReadBuffer(GL_BACK);
		unsigned char color_value[3];
		glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, color_value);
		
		// Decode the packed 24-bit index from the read-back RGB
		int r = (int)color_value[0];
		int g = (int)color_value[1];
		int b = (int)color_value[2];
		
		int idx = (r << 16) | (g << 8) | b;
		int texW = image.GetWidth();
		int texH = image.GetHeight();
		int px_tex = idx % texW;
		int py_tex = idx / texW;
		
		glm::vec2 p1_tex = glm::vec2((float)px_tex, (float)py_tex);
	
	///p1_2d = glm::vec2(xpos,ypos);
	/*
	int  win_width, win_height;
	glfwGetWindowSize(window,&win_width, &win_height);
	auto p1_st = glm::vec2(p1_2d.x / win_width, 1 - (p1_2d.y/win_height)); 
	auto p0_st = glm::vec2(p0_2d.x / win_width, 1 - (p0_2d.y/win_height)); 
	auto p0_img = glm::vec2((float)image.GetWidth()*p0_st.x,(float)image.GetHeight()*p0_st.y); 
	auto p1_img = glm::vec2((float)image.GetWidth()*p1_st.x,(float)image.GetHeight()*p1_st.y);
	*/
	dda(p0_2d,p1_tex,"stroke"); // implemento DDA en 3d
	p0_2d = p1_tex;
	texture.update(image);
	}
}

void dda(glm::vec2 p_0,glm::vec2 p_1,std::string type)
{
    float dx = p_1.x - p_0.x;
    float dy = p_1.y - p_0.y;

    if (dx == 0 && dy == 0) return;

    if (fabs(dx) > fabs(dy)) {
        // caso x-dominante
        if (p_0.x > p_1.x) std::swap(p_0, p_1);

        float slope = dy / dx;
        float y = p_0.y;
        for (int x = (int)p_0.x; x <= (int)p_1.x; x++) {
			if(type == "line") blendPixel((int)round(y), x);
			if(type == "stroke" /* && x % (int)(radius/2) == 0 */) drawCircle(radius,glm::vec2(x,(int)round(y)));	
            y += slope;
        }
    } else {
        // caso y-dominante
        if (p_0.y > p_1.y) std::swap(p_0, p_1);

        float slope = dx / dy;
        float x = p_0.x;
        for (int y = (int)p_0.y; y <= (int)p_1.y; y++) {
			if(type == "line") blendPixel(y, (int)round(x));
			if(type == "stroke"  /* && y % (int)(radius/2) == 0 */ ) drawCircle(radius,glm::vec2((int)round(x),y));	
            x += slope;
        }
    }
}

void drawCircle(int r,glm::vec2 cp)
{
	// Bounding box
	auto min_x = cp.x-r;
	auto max_x = cp.x+r;
	auto min_y = cp.y-r;
	auto max_y = cp.y+r;

	for (auto x = min_x; x < max_x; x++)
	{
		bool inside_flag = false;
		for (auto y = min_y; y < max_y; y++)
		{
			auto p = glm::vec2(x,y);
			auto v = cp-p;
			if (glm::pow(r,2) >= (glm::pow(v.x,2)+ glm::pow(v.y,2)))
			{
				blendPixel(p.y,p.x);
			}
		}
	}
	// Para radios pequeños:
	for (int x = min_x; x <= max_x; ++x) {
		for (int y = min_y; y <= max_y; ++y) {
			glm::vec2 p((float)x, (float)y);
			glm::vec2 v = cp - p;
			if ((v.x * v.x + v.y * v.y) <= (float)r * (float)r) {
				blendPixel(y, x);
			}
    }
}
}

void getColorCoordinates(GLFWwindow* window_2d,glm::vec2 p)
{
	int  win_width, win_height;
    glfwGetWindowSize(window_2d,&win_width, &win_height);
	auto p_st = glm::vec2(p.x / win_width, 1 - (p.y/win_height)); 
	auto p_img = glm::vec2((float)image.GetWidth()*p_st.x,(float)image.GetHeight()*p_st.y);


}

void makeColorMap() {
    colorToImage.clear();
    imageToColor.clear();

    int width = image.GetWidth();
    int height = image.GetHeight();

    unsigned int r = 0, g = 0, b = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
			

            // crear color actual
            glm::vec3 color_rgb = glm::vec3(
                (int)r,
                (int)g,
                (int)b
            );

            glm::vec2 pos = glm::vec2((float)x,(float)(y));

            colorToImage[color_rgb] = pos;
            imageToColor[pos] = color_rgb;


            image_colormap.SetRGB(y, x,glm::vec3((r / 255.0f),(g / 255.0f),(b / 255.0f)));
			// image_colormap.SetRGB(y, x, glm::vec3(0,0,0));

            // avanzar el “contador” de color
            b+=1;
            if (b >= 256) {
                b = 0;
                g+=1;
                if (g >= 256) {
                    g = 0;
                    r+=1;
                    if (r >= 256) r = 0;
				}
			}
        }
    }
	std::cout << "color: " << r << ", " << g << ", " << b << std::endl;
}

void blendPixel(int y, int x) {
	
	auto img_width = image.GetWidth();
	auto img_height = image.GetHeight();
	
	if (x < img_width and y < img_height and x > 0 and y > 0){ // para no pintar fuera de la imagen
		
        glm::vec3 current_color = image.GetRGB(y, x); 
        float alpha = color.w;

        glm::vec3 src = glm::vec3(color.x, color.y, color.z);
        glm::vec3 dst = current_color;
		
		//alpha = alpha/(radius/5); // para que el alpha en radios grandes ande 
		
		// pero evitar alpha > 1
    	//float scaled = alpha / (radius / 5.0f);
    	//alpha = std::min(1.0f, scaled);
		
        glm::vec3 out = alpha * src + (1.0f - alpha) * dst;
        image.SetRGB(y,x, out);
		
	}
};


void printColorToImage() {
    std::cout << "colorToImage:" << std::endl;
    for (const auto& pair : colorToImage) {
        const glm::vec3& color = pair.first;
        const glm::vec2& pos = pair.second;
        std::cout << "Color (" 
                  << color.x << ", " << color.y << ", " << color.z << ")"
                  << " -> Pos (" 
                  << pos.x << ", " << pos.y << ")" << std::endl;
    }
}

void printImageToColor() {
    std::cout << "imageToColor:" << std::endl;
    for (const auto& pair : imageToColor) {
        const glm::vec2& pos = pair.first;
        const glm::vec3& color = pair.second;
        std::cout << "Pos (" 
                  << pos.x << ", " << pos.y << ")"
                  << " -> Color (" 
                  << color.x << ", " << color.y << ", " << color.z << ")" << std::endl;
    }
}
