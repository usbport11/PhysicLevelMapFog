#include "stdafx.h"
#include "classes/system/Shader.h"
#include "classes/system/Scene.h"
#include "classes/system/FPSController.h"

#include "classes/physic/PhysicWorld.h"
#include "classes/physic/objects/Hero.h"
#include "classes/physic/objects/Enemy.h"
#include "classes/physic/objects/Bullet.h"
#include "classes/physic/objects/Exit.h"
#include "classes/physic/objects/Doodad.h"

#include "classes/level/Leaf.h"
#include "classes/delaunay/delaunay.h"
#include "classes/level/TileMap.h"
#include "classes/buffers/AtlasBuffer.h"
#include "classes/buffers/FrameBuffer.h"
#include "classes/buffers/StaticBuffer.h"
#include "classes/image/TextureLoader.h"

bool Pause;
bool keys[1024] = {0};
int WindowWidth = 800, WindowHeight = 600;
bool EnableVsync = 1;
GLFWwindow* window;
stFPSController FPSController;

MShader Shader;
MShader ShaderLight;
MScene Scene;

MTextureLoader TextureLoader;
stTexture* txAtlas = NULL;
stTexture* txLight = NULL;
stTexture* txObjects = NULL;
unsigned int txOne_cnt;

MPhysicWorld PhysicWorld;
MHero* Hero = NULL;
MEnemy* Enemy = NULL;
MExit* Exit = NULL;
glm::vec2 HeroVelocity = glm::vec2(80.0, 80.0);//2,2
glm::vec2 BulletVelocity = glm::vec2(4.0, 4.0);
glm::vec2 HeroSize = glm::vec2(32, 32);
glm::vec2 ExitSize = glm::vec2(64, 64);

//mouse data
bool UseCamera = true;
bool UseFog = true;
bool UseBlending = false;
glm::vec2 MouseSceneCoord;
double mx, my;
glm::vec2 ObjectDirection;
float ObjectRotation;

MAtlasBuffer AtlasBuffer;
MTileMap TileMap;
int TilesCount[2] = {30, 30};
glm::vec2 TileSize(64, 64);
glm::vec2 CameraPosition;
glm::vec2 OldPosition;

MTextureQuadBuffer LightBuffer;
MFrameBuffer FrameBuffer;
glm::vec4 AmbientColor = glm::vec4(0.3f, 0.3f, 0.3f, 0.3f);//0.3,0.3,0.7,0.7
glm::vec4 WhiteColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
stTextureQuad LightQuad;
glm::vec2 LightSize = glm::vec2(400, 400);

bool GenerateLevel();

static void error_callback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
}

static void mousepos_callback(GLFWwindow* window, double x, double y) {
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if(action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_1 && !Pause) {//IMPORTANT!!! fix mouse double click 
		MBullet* Bullet = new MBullet;
    	Bullet->Set(glm::vec2(Hero->GetCenter().x - 10, Hero->GetCenter().y - 10), glm::vec2(20, 20), glm::vec2(0, 0.25), glm::vec2(0.25, 0.25), OT_ENEMY | OT_DOODAD);
    	if(!PhysicWorld.AddPhysicQuad((MPhysicQuad*)Bullet)) return;
    	glm::vec2 BulletDirection = glm::normalize(MouseSceneCoord - Hero->GetCenter());
    	float BulletRotation = atan2(BulletDirection.y, BulletDirection.x);
    	Bullet->SetRotationAngle(BulletRotation);
    	Bullet->SetVelocity(b2Vec2(BulletDirection.x * BulletVelocity.x, BulletDirection.y * BulletVelocity.y));
    	PhysicWorld.FillQuadBuffer();
	}
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    	Pause = true;
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		return;
	}
	//regenerate level
	if(key == 'R' && action == GLFW_PRESS) {
		Pause = true;
		if(!GenerateLevel()) glfwSetWindowShouldClose(window, GLFW_TRUE);
		Pause = false;
		return;
	}
	if(key == 'C' && action == GLFW_PRESS) {
		UseCamera = !UseCamera;
		CameraPosition.x = (int)Hero->GetCenter().x;
		CameraPosition.y = (int)Hero->GetCenter().y;
		OldPosition = Hero->GetCenter();
    	Scene.ViewAt(CameraPosition);//start view center
    	return;
	}
	if(key == 'F' && action == GLFW_PRESS) {
		UseFog = !UseFog;
    	return;
	}
	if(key == 'B' && action == GLFW_PRESS) {
		UseBlending = !UseBlending;
		if(UseBlending) glEnable(GL_BLEND);
		else glDisable(GL_BLEND);
    	return;
	}
	
	if(action == GLFW_PRESS)
    	keys[key] = true;
    else if (action == GLFW_RELEASE)
    	keys[key] = false;
}

void MoveKeysPressed() {
	b2Vec2 CurrentVelocity = b2Vec2(0.0f, 0.0f);
	if(keys['A'] || keys['D'] || keys['W'] || keys['S']) {
		if(keys['A']) {
			CurrentVelocity.x = -HeroVelocity.x;
		}
		if(keys['D']) {
			CurrentVelocity.x = HeroVelocity.x ;
		}
		if(keys['W']) {
			CurrentVelocity.y = HeroVelocity.y;
		}
		if(keys['S']) {
			CurrentVelocity.y = -HeroVelocity.y;
		}
	}
	CurrentVelocity.x *= FPSController.DeltaFrameTime;
	CurrentVelocity.y *= FPSController.DeltaFrameTime;
	if(Hero) Hero->SetVelocity(CurrentVelocity); 
}

bool CreatePhysicWalls() {
	vector<NLine2> Lines;
	
	NLine2 Line1, Line2;
	//get horizontal walls
	for(int i=0; i<TilesCount[0]; i++) {
		for(int j=0; j<TilesCount[1] - 1; j++) {
			//top walls
			if(TileMap.GetValue(i, j) != TT_FLOOR && TileMap.GetValue(i, j+1) == TT_FLOOR) {
				if(!Line1.a.x) {
					Line1.a.x = i;
					Line1.b.x = i;
					Line1.a.y = j+1;
					Line1.b.y = j+1;
				}
				else Line1.b.x ++;
			}
			else {
				if(Line1.a.x) {
					Line1.b.x ++;
					Lines.push_back(Line1);
					Line1.a.x = 0;
				}
			}
			//bottom walls
			if(TileMap.GetValue(i, j) == TT_FLOOR && TileMap.GetValue(i, j+1) != TT_FLOOR) {
				if(!Line2.a.x ) {
					Line2.a.x = i;
					Line2.b.x = i;
					Line2.a.y = j+1;
					Line2.b.y = j+1;
				}
				else Line2.b.x ++;
			}
			else {
				if(Line2.a.x) {
					Line2.b.x ++;
					Lines.push_back(Line2);
					Line2.a.x = 0;
				}
			}
		}
	}

	//get vertical walls
	for(int i=0; i<TilesCount[0] - 1; i++) {
		for(int j=0; j<TilesCount[1]; j++) {
			//left walls
			if(TileMap.GetValue(i, j) != TT_FLOOR && TileMap.GetValue(i+1, j) == TT_FLOOR) {
				if(!Line1.a.y) {
					Line1.a.y = j;
					Line1.b.y = j;
					Line1.a.x = i+1;
					Line1.b.x = i+1;
				}
				else Line1.b.y ++;
			}
			else {
				if(Line1.a.y) {
					Line1.b.y ++;
					Lines.push_back(Line1);
					Line1.a.y = 0;
				}
			}
			//right walls
			if(TileMap.GetValue(i, j) == TT_FLOOR && TileMap.GetValue(i+1, j) != TT_FLOOR) {
				if(!Line2.a.y ) {
					Line2.a.y = j;
					Line2.b.y = j;
					Line2.a.x = i+1;
					Line2.b.x = i+1;
				}
				else Line2.b.y ++;
			}
			else {
				if(Line2.a.y) {
					Line2.b.y ++;
					Lines.push_back(Line2);
					Line2.a.y = 0;
				}
			}
		}
	}
	
	for(int i=0; i<Lines.size(); i++) {
		if(Lines[i].a.x) {
			if(!PhysicWorld.AddWall(b2Vec2(Lines[i].a.x * TileSize.x, Lines[i].a.y * TileSize.y), b2Vec2(Lines[i].b.x * TileSize.x, Lines[i].b.y * TileSize.y))) return false;
		}
	}
	
	Lines.clear();
	
	return true;
}

bool FillTileBuffer() {
	unsigned int AtasPos[2];
	for(int i=0; i<TilesCount[0] - 1; i++) {
		for(int j=0; j<TilesCount[1] - 1; j++) {
			if(TileMap.GetValue(i, j) == TT_NONE) continue;
			if(TileMap.GetValue(i, j) == TT_FLOOR) {
				AtasPos[0] = 0;
				AtasPos[1] = 1;
			}
			if(TileMap.GetValue(i, j) == TT_WALL_FULL) {
				AtasPos[0] = 1;
				AtasPos[1] = 0;
			}
			if(TileMap.GetValue(i, j) == TT_WALL_PART) {
				AtasPos[0] = 0;
				AtasPos[1] = 0;
			}
			if(!AtlasBuffer.AddData(glm::vec2(i * TileSize.x, j * TileSize.y), glm::vec2((i + 1) * TileSize.x, (j + 1) * TileSize.y), AtasPos[0], AtasPos[1], 0, true)) return false;
		}
	}
	return true;
}

bool GenerateLevel() {
	LogFile<<"Level: Start generate new level. Clear previous data"<<endl;
	
	//clear data before fill new
	AtlasBuffer.Clear();
	TileMap.Clear();
	PhysicWorld.Clear();
	
	glm::vec2 EndPoint;
	glm::vec2 StartPoint;
	
	list<TNode<stLeaf>* > Tree;
	int MinLeafSize = 6;
	int MaxLeafSize = 20;
	int MinRoomSize = 3;
	
	LogFile<<"Level: Create tree: "<<MinLeafSize<<" "<<MaxLeafSize<<" "<<MinRoomSize<<endl;
	//create tree
	if(MinRoomSize >= MinLeafSize || MinLeafSize >= MaxLeafSize) {
		cout<<"Wrong settings"<<endl;
		return 0;
	}
	if(!SplitTree(&Tree, TilesCount[0], TilesCount[1], MinLeafSize, MaxLeafSize)) return false;
	
	LogFile<<"Level: Create rooms"<<endl;
	//create rooms and fill centers map
	float Distance, MaxDistance = 0;
	glm::vec3 Color = glm::vec3(1, 1, 1);
	TNode<NRectangle2>* pRoomNode;
	map<glm::vec2, TNode<NRectangle2>*, stVec2Compare> NodesCenters;
	glm::vec2 Center;
	NRectangle2* pRoom;
	list<TNode<stLeaf>* >::iterator itl;
	int RoomsNumber = 0;
	for(itl = Tree.begin(); itl != Tree.end(); itl++) {
		pRoomNode = CreateRoomInLeaf(*itl, MinRoomSize);
		if(!pRoomNode) continue;
		pRoom = pRoomNode->GetValueP();
		if(!pRoom) continue;
		//add in map
		Center.x = (pRoom->Position.x + pRoom->Size.x * 0.5) * TileSize.x;
		Center.y = (pRoom->Position.y + pRoom->Size.y * 0.5) * TileSize.y;
		NodesCenters.insert(pair<glm::vec2, TNode<NRectangle2>* >(Center, pRoomNode));
		//add in buffer
		RoomsNumber ++;
		TileMap.SetRectangle(*pRoom, 1);
		//get min distance
		if(NodesCenters.size() == 1) {
			EndPoint = StartPoint = Center;
		}
		else {
			Distance = glm::distance(Center, StartPoint);
			if(Distance > MaxDistance) {
				MaxDistance = Distance;
				EndPoint = Center;
			}
		}
	}
	if(RoomsNumber < 2) {
		cout<<"Too few rooms: "<<RoomsNumber<<endl;
		return false;
	}
	//rnd swap of points
	if(rand() % 2) {
		glm::vec2 Save = StartPoint;
		StartPoint = EndPoint;
		EndPoint = Save;
	}
	
	//copy centers for tringualtion
	//fill doodads buffer (must be created before halls)
	//don't forget skip start level position
	map<glm::vec2, TNode<NRectangle2>*, stVec2Compare>::iterator itm;
	vector<glm::vec2> CentersPoints;
	int RoomSquare = 0;
	int RndCount = 0;
	int CurrentCount = 0;
	int DoodadsLimit = 0;
	vector<glm::vec2> DoodadsCenters;
	for(itm = NodesCenters.begin(); itm != NodesCenters.end(); itm++) {
		CentersPoints.push_back(itm->first);
		//if room is not start room and not end room
		if(itm->first != StartPoint && itm->first != EndPoint) {
			//get room size (width * height)
			pRoom = itm->second->GetValueP();
			RoomSquare = pRoom->Size.x * pRoom->Size.y;
			//rand from this number
			RndCount = rand() % (RoomSquare);
			if(!RndCount) continue;
			//place doodads in this points
			CurrentCount = 0;
			DoodadsLimit = rand() % (RoomSquare / 2) + 1;
			for(int i=pRoom->Position.x; i<pRoom->Position.x + pRoom->Size.x; i++) {
				for(int j=pRoom->Position.y; j<pRoom->Position.y + pRoom->Size.y; j++) {
					if(CurrentCount >= RndCount || CurrentCount >= DoodadsLimit) break;
					if(rand() % 2) {
						DoodadsCenters.push_back(glm::vec2(i * TileSize.x + TileSize.x / 2, j * TileSize.y + TileSize.y / 2));
						CurrentCount ++;
					}
				}
			}
		}
	}
	
	LogFile<<"Level: Triangulate using rooms centers. Get MST"<<endl;
	//triangulate by delaunay and get mst
	MDelaunay Triangulation;
	vector<MTriangle> Triangles = Triangulation.Triangulate(CentersPoints);
	vector<MEdge> Edges = Triangulation.GetEdges();
	vector<MEdge> MST = Triangulation.CreateMSTEdges();
	
	LogFile<<"Level: Create halls"<<endl;
	//create halls
	vector<NRectangle2> Halls;
	TNode<NRectangle2>* pNode0;
	TNode<NRectangle2>* pNode1;
	for(int i=0; i<MST.size(); i++) {
		pNode0 = NodesCenters[MST[i].p1];
		pNode1 = NodesCenters[MST[i].p2];
		Halls = CreateHalls2(pNode0->GetValueP(), pNode1->GetValueP());
		for(int k=0; k<Halls.size(); k++) {
			TileMap.SetRectangle(Halls[k], 1);
		}
	}
	
	LogFile<<"Level: Clear buffers"<<endl;
	Halls.clear();
	MST.clear();
	Triangulation.Clear();
	Triangles.clear();
	Edges.clear();
	CentersPoints.clear();

	NodesCenters.clear();
	
	ClearTree(&Tree);
	
	LogFile<<"Level: Create visual part"<<endl;
	//later create floor
	TileMap.CreateWalls();
	//fill buffer based on 
	FillTileBuffer();
	//dispose buffer
	if(!AtlasBuffer.Dispose()) return false;
	
	LogFile<<"Level: Create physic part (walls, exit, hero, etc)"<<endl;
	//create physic walls
	if(!PhysicWorld.CreateLevelBody()) return false;
	if(!CreatePhysicWalls()) return false;
    Hero = new MHero;
    Exit = new MExit;
    if(!Hero->Set(StartPoint, HeroSize, glm::vec2(0, 0), glm::vec2(0.25, 0.25))) return false;
    if(!Exit->Set(EndPoint - glm::vec2(ExitSize.x / 2, ExitSize.y /2), ExitSize, glm::vec2(0.25, 0), glm::vec2(0.25, 0.25), LET_NEXT)) return false;
    if(!PhysicWorld.AddPhysicQuad((MPhysicQuad*)Exit)) return false;
    if(!PhysicWorld.AddPhysicQuad((MPhysicQuad*)Hero)) return false;
    MDoodad* Doodad;
    int Diff[2];
    for(int i=0; i<DoodadsCenters.size(); i++) {
    	Doodad = new MDoodad;
    	Diff[0] = TileSize.x - HeroSize.x;
    	Diff[1] = TileSize.y - HeroSize.y;
    	if(!Doodad->Set(DoodadsCenters[i] - glm::vec2(rand() % Diff[0], rand() % Diff[1]), HeroSize, glm::vec2(0.5 + (rand() % 2) * 0.25, 0), glm::vec2(0.25, 0.25))) return false;
    	if(!PhysicWorld.AddPhysicQuad((MPhysicQuad*)Doodad)) return false;
	}
	DoodadsCenters.clear();
    PhysicWorld.FillQuadBuffer();
    
	LogFile<<"Level: Done"<<endl;
	
	return true;
}

bool InitApp() {
	LogFile<<"Starting application"<<endl;    
    glfwSetErrorCallback(error_callback);
    
    if(!glfwInit()) return false;
    window = glfwCreateWindow(WindowWidth, WindowHeight, "TestApp", NULL, NULL);
    if(!window) {
        glfwTerminate();
        return false;
    }
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mousepos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwMakeContextCurrent(window);
    if(glfwExtensionSupported("WGL_EXT_swap_control")) {
    	LogFile<<"Window: V-Sync supported. V-Sync: "<<EnableVsync<<endl;
		glfwSwapInterval(EnableVsync);//0 - disable, 1 - enable
	}
	else LogFile<<"Window: V-Sync not supported"<<endl;
    LogFile<<"Window created: width: "<<WindowWidth<<" height: "<<WindowHeight<<endl;

	//glew
	GLenum Error = glewInit();
	if(GLEW_OK != Error) {
		LogFile<<"Window: GLEW Loader error: "<<glewGetErrorString(Error)<<endl;
		return false;
	}
	LogFile<<"GLEW initialized"<<endl;
	
	if(!CheckOpenglSupport()) return false;

	//shaders
	if(!Shader.CreateShaderProgram("shaders/main.vertexshader.glsl", "shaders/main.fragmentshader.glsl")) return false;
	if(!Shader.AddUnifrom("MVP", "MVP")) return false;
	
	if(!ShaderLight.CreateShaderProgram("shaders/light.vertexshader.glsl", "shaders/light.fragmentshader.glsl")) return false;
	if(!ShaderLight.AddUnifrom("MainTexture", "mainTexture")) return false;
	if(!ShaderLight.AddUnifrom("LightTexture", "lightTexture")) return false;
	if(!ShaderLight.AddUnifrom("Resolution", "resolution")) return false;
	if(!ShaderLight.AddUnifrom("AmbientColor", "ambientColor")) return false;
	LogFile<<"Shaders loaded"<<endl;

	//blending function
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//scene
	if(!Scene.Initialize(&WindowWidth, &WindowHeight)) return false;
	LogFile<<"Scene initialized"<<endl;

	//randomize
    srand(time(NULL));
    LogFile<<"Randomized"<<endl;
    
    //other initializations 
    //load texture
    txAtlas = TextureLoader.LoadTexture("textures/tex04.png", 1, 1, 0, txOne_cnt, GL_NEAREST, GL_REPEAT);
    if(!txAtlas) return false;
	txLight = TextureLoader.LoadTexture("textures/tex06.png", 1, 1, 0, txOne_cnt, GL_NEAREST, GL_REPEAT);
    if(!txLight) return false;
    txObjects = TextureLoader.LoadTexture("textures/tex10.png", 1, 1, 0, txOne_cnt, GL_NEAREST, GL_REPEAT);
    if(!txObjects) return false;
    
    if(!FrameBuffer.Initialize(WindowWidth, WindowHeight)) return false;
    
    //prepare level data
	if(!AtlasBuffer.Initialize(txAtlas, 32, 32, 2, 2)) return false;
	//if(!DoodadsBuffer.Initialize(txDoodads, 32, 32, 2, 2)) return false;
    if(!TileMap.Initialize(TilesCount[0], TilesCount[1])) return false;
    if(!PhysicWorld.Initialize(0.0f, (float)1/60, 8, 3, 0.01f, txObjects->Id)) return false;
    //generate level (first time)
	if(!GenerateLevel()) return false;
    
    if(!LightBuffer.Initialize(GL_STREAM_DRAW, txLight->Id)) return false;
    LightQuad = stTextureQuad(glm::vec2(0, 0), LightSize, glm::vec2(0, 0), glm::vec2(1, 1));
	if(!LightBuffer.AddQuad(&LightQuad)) return false;
	LightBuffer.Relocate();
	
	//turn off pause
	Pause = false;
    
    return true;
}

void PreRenderStep() {
	if(Pause) return;
	
	//direction move
	//having some twiching while use with camera
	glfwGetCursorPos(window, &mx, &my);
	MouseSceneCoord = Scene.WindowPosToWorldPos(mx, my);
	ObjectDirection = glm::normalize(MouseSceneCoord - Hero->GetCenter());
	ObjectRotation = atan2(ObjectDirection.y, ObjectDirection.x);
	Hero->SetRotationAngle(ObjectRotation);
	
	MoveKeysPressed();
	
	PhysicWorld.Step();
	PhysicWorld.UpdateObjects();
	LightQuad = stTextureQuad(glm::vec2(Hero->GetCenter().x - LightSize.x / 2, Hero->GetCenter().y - LightSize.y / 2), LightSize, glm::vec2(0, 0), glm::vec2(1, 1));
	LightBuffer.UpdateAll();
	
	//update camera only in case that hero position is changed
	//if camera use float point while moving textures may glitch
	if(UseCamera) {
		if(OldPosition != Hero->GetCenter()) {
			CameraPosition.x = Hero->GetCenter().x;
			CameraPosition.y = Hero->GetCenter().y;
			Scene.ViewAt(CameraPosition);
	    	OldPosition = Hero->GetCenter();
		}
	}
}

//using if statement in render not good (maybe)
void RenderStep() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0, 0, 0, 0);
	
	//if statement in render function may be not good decision
	if(UseFog) {
		glUseProgram(ShaderLight.ProgramId);
		glUniformMatrix4fv(ShaderLight.Uniforms["MVP"], 1, GL_FALSE, Scene.GetDynamicMVP());
		glUniform1i(ShaderLight.Uniforms["MainTexture"], 0);
		glUniform1i(ShaderLight.Uniforms["LightTexture"], 1);
		glUniform2f(ShaderLight.Uniforms["Resolution"], WindowWidth, WindowHeight);
		glUniform4fv(ShaderLight.Uniforms["AmbientColor"], 1, &WhiteColor[0]);
		FrameBuffer.Begin();
		LightBuffer.Begin();
		LightBuffer.DrawAll();
		LightBuffer.End();
		FrameBuffer.End();
		glUniform4fv(ShaderLight.Uniforms["AmbientColor"], 1, &AmbientColor[0]);
		AtlasBuffer.Begin();
		FrameBuffer.Bind(1);
		AtlasBuffer.Draw();
		AtlasBuffer.End();
		PhysicWorld.DrawQuadBuffer();
	}
	else {
		glUseProgram(Shader.ProgramId);
		glUniformMatrix4fv(Shader.Uniforms["MVP"], 1, GL_FALSE, Scene.GetDynamicMVP());
		AtlasBuffer.Begin();
		AtlasBuffer.Draw();
		AtlasBuffer.End();
		PhysicWorld.DrawQuadBuffer();
	}
}

void ClearApp() {
	//clear funstions
	LightBuffer.Close();
	FrameBuffer.Close();
	PhysicWorld.Close();
	AtlasBuffer.Close();
	TileMap.Close();
	TextureLoader.DeleteTexture(txAtlas, txOne_cnt);
	TextureLoader.DeleteTexture(txLight, txOne_cnt);
	TextureLoader.DeleteTexture(txObjects, txOne_cnt);
	TextureLoader.Close();
	
	memset(keys, 0, 1024);
	Shader.Close();
	ShaderLight.Close();
	LogFile<<"Application: closed"<<endl;
}

int main(int argc, char** argv) {
	LogFile<<"Application: started"<<endl;
	if(!InitApp()) {
		ClearApp();
		glfwTerminate();
		LogFile.close();
		return 0;
	}
	FPSController.Initialize(glfwGetTime());
	while(!glfwWindowShouldClose(window)) {
		FPSController.FrameStep(glfwGetTime());
    	FPSController.FrameCheck();
    	PreRenderStep();
		RenderStep();
        glfwSwapBuffers(window);
        glfwPollEvents();
	}
	ClearApp();
    glfwTerminate();
    LogFile.close();
}
