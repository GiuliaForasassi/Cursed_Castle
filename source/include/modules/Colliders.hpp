
//enum colliderType {CLD_POINT, CLD_SEGMENT, CLD_RECT, CLD_AABB, CLD_OOBB, CLD_SPHERE, CLD_CAPSULE, CLD_BVH;

// Collider types actually supported by the module
enum colliderType {CLD_POINT, CLD_AABB, CLD_OOBB, CLD_SPHERE, CLD_BVH};

// Struct containing the 8 vertices of a bounding box (AABB or OOBB)
struct EightPoints {
	glm::vec3 P[8];
};

// Struct containing the extents of an AABB: minimum and maximum on each axis
struct AABBextents {
	float xMin, xMax;
	float yMin, yMax;
	float zMin, zMax;
};

class ColliderShow;

// Main class representing a generic collider
class Collider {
	friend ColliderShow;

	colliderType type; // AABB or OOBB or POINT or SPHERE or BVH

	// Geometric parameters:
	// AABB/OOBB: minimum corner (x1,x2,x3) and maximum corner (x2,y2,z2)
	// POINT: position (x1,y1,z1)
	// SPHERE: center (x1,y1,z1) and radius (r)
	float x1, y1, z1;
	float x2, y2, z2;
	float r;

	// BVH: list of the children colliders belonging to the BVH node
	std::vector<Collider *> children;

	// World matrix of the collider (position, rotation, scale)
	glm::mat4 Wm;

	// Utility functions
	void transformAABB(EightPoints &Ps, Collider &C);
	void getExtentsAABB(EightPoints &Ps, AABBextents &E);
	glm::vec3 getClosestPointOnAABB(AABBextents &E, glm::vec3 P);
	float getTransformedRadius(Collider &C);
	void getMinMaxAlongAxes(EightPoints P, glm::vec3 Axis, float &Min, float &Max);
	bool checkIntersectAxis(EightPoints P1, EightPoints P2, glm::vec3 Axis);
	void getModelExtentsAABB(Model *M, AABBextents &E);

	// Collisions functions
	bool collisionPointSphere(Collider &P1l, Collider &P2l);
	bool collisionSphereSphere(Collider &P1l, Collider &P2l);
	bool collisionPointPoint(Collider &P1l, Collider &P2l);
	bool collisionPointAABB(Collider &P1l, Collider &P2l);
	bool collisionPointOOBB(Collider &P1l, Collider &P2l);
	bool collisionAABBAABB(Collider &P1l, Collider &P2l);
	bool collisionOOBBAABB(Collider &P1l, Collider &P2l);
	bool collisionOOBBOOBB(Collider &P1l, Collider &P2l);
	bool collisionSphereAABB(Collider &P1l, Collider &P2l);
	bool collisionSphereOOBB(Collider &P1l, Collider &P2l);
	bool collisionBVHCollider(Collider &P1l, Collider &P2l);

	public:
	// Initialization functions
	void initPoint(float x, float y, float z);
//	void initSegment(float _x1, float _y1, float _z1, float _x2, float _y2, float _z2);
//	void initRect(float _x1, float _y1, float _x2, float _y2);
	void initAABB(float _x1, float _y1, float _z1, float _x2, float _y2, float _z2);
	void initOOBB(float _x1, float _y1, float _z1, float _x2, float _y2, float _z2);
	void fitAABB(Model *M);
	void fitOOBB(Model *M);
	void initSphere(float x, float y, float z, float _r);
//	void initCapsule(float _x1, float _y1, float _z1, float _x2, float _y2, float _z2, float _r);
	void initBVH(std::vector<Collider *> _c);

	// Other functions
	void setWorldMatrix(glm::mat4 M);
	bool collidesWith(Collider &dest);
	AABBextents getExtents();
};

// Initialize a POINT collider
inline void Collider::initPoint(float x, float y, float z) {
	type = CLD_POINT;
	Wm = glm::mat4(1);
	children = {};

	// Set the position of the point
	x1 = x; y1 = y; z1 = z;
}

/*
void Collider::initSegment(float _x1, float _y1, float _z1, float _x2, float _y2, float _z2) {
	type = CLD_SEGMENT;
	Wm = glm::mat4(1);
	children = {};
	
	x1 = _x1; y1 = _y1; z1 = _z1;
	x2 = _x2; y2 = _y2; z2 = _z2;
}

void Collider::initRect(float _x1, float _y1, float _x2, float _y2) {
	type = CLD_RECT;
	Wm = glm::mat4(1);
	children = {};
	
	x1 = _x1; y1 = _y1;
	x2 = _x2; y2 = _y2;
}*/

// Initialize an AABB Collider
inline void Collider::initAABB(float _x1, float _y1, float _z1, float _x2, float _y2, float _z2) {
	type = CLD_AABB;
	Wm = glm::mat4(1);
	children = {};

	// Set local AABB extents
	x1 = _x1; y1 = _y1; z1 = _z1;
	x2 = _x2; y2 = _y2; z2 = _z2;
}

// Initialize an OOBB Collider
inline void Collider::initOOBB(float _x1, float _y1, float _z1, float _x2, float _y2, float _z2) {
	type = CLD_OOBB;
	Wm = glm::mat4(1);
	children = {};

	// Set local AABB extents
	// Note: locally, an OOBB is like an AABB, the orientation comes from the Wm
	x1 = _x1; y1 = _y1; z1 = _z1;
	x2 = _x2; y2 = _y2; z2 = _z2;
}

// Compute the model's AABB extents from its vertices
inline void Collider::getModelExtentsAABB(Model *M, AABBextents &E) {
	VertexDescriptor *VD = M->getVD();
	
	if(!VD->Position.hasIt) {
		std::cout << "Error! Vertex descriptor does not have position. Cannot create Bounding Box\n";
		return;
	}
	
	int offset = VD->Position.offset;		// Offset of position inside vertex's structure
	int stride = VD->Bindings[0].stride;	// Size of a vertex in bytes

// std::cout << stride << " " << offset << " " << M->vertices.size() << " " << (M->vertices.size() / stride) << "\n";
	for(int startPos = 0; startPos < M->vertices.size(); startPos += stride) {
		glm::vec3 *pos = (glm::vec3 *)((char*)(&M->vertices[startPos]) + offset);

		// First vertex: initialize min/max
		if(startPos == 0) {
			E.xMin = E.xMax = pos->x;
			E.yMin = E.yMax = pos->y;
			E.zMin = E.zMax = pos->z;
		} else {
			// Update min/max
			if(pos->x < E.xMin) {E.xMin = pos->x;}
			if(pos->x > E.xMax) {E.xMax = pos->x;}
			if(pos->y < E.yMin) {E.yMin = pos->y;}
			if(pos->y > E.yMax) {E.yMax = pos->y;}
			if(pos->z < E.zMin) {E.zMin = pos->z;}
			if(pos->z > E.zMax) {E.zMax = pos->z;}
		}
//		std::cout << pos->x << ", " << pos->y << ", " << pos->z << "\n";
	}
//	std::cout << E.xMin << " " << E.yMin << " " << E.zMin << "    " << E.xMax << " " << E.yMax << " " << E.zMax << "\n";
}

// Initialize an AABB enclosing a model
inline void Collider::fitAABB(Model *M) {
	AABBextents E;
	
	getModelExtentsAABB(M, E);
	initAABB(E.xMin, E.yMin, E.zMin, E.xMax, E.yMax, E.zMax);
}

// Initialize an OOBB enclosing a model
// Note: this returns a non-oriented box in local space.
//		 Rotation must be applied by setting its world matrix.
inline void Collider::fitOOBB(Model *M) {
	AABBextents E;
	
	getModelExtentsAABB(M, E);
	initOOBB(E.xMin, E.yMin, E.zMin, E.xMax, E.yMax, E.zMax);
}

// Initialize a SPHERE collider
inline void Collider::initSphere(float x, float y, float z, float _r) {
	type = CLD_SPHERE;
	Wm = glm::mat4(1);
	children = {};

	// Set the center of the sphere
	x1 = x; y1 = y; z1 = z;
	// Set the radius of the sphere
	r = _r;
}

/*
void Collider::initCapsule(float _x1, float _y1, float _z1, float _x2, float _y2, float _z2, float _r) {
	type = CLD_CAPSULE;
	Wm = glm::mat4(1);
	
	x1 = _x1; y1 = _y1; z1 = _z1;
	x2 = _x2; y2 = _y2; z2 = _z2;
	r = _r;
}*/

// Initialize a BVH node computing the AABB that encloses all the children
inline void Collider::initBVH(std::vector<Collider *> _c) {
	type = CLD_BVH;
	Wm = glm::mat4(1);
	children = _c;


	if(children.empty()) return;

	// Initialize the AABB with the first child
	AABBextents E = children[0]->getExtents();

	// Update the AABB with the rest of the children
	for(int i = 1; i < children.size(); i++)
	{
		AABBextents Ei = children[i]->getExtents();
		E.xMin = std::min(E.xMin, Ei.xMin);
		E.xMax = std::max(E.xMax, Ei.xMax);
		E.yMin = std::min(E.yMin, Ei.yMin);
		E.yMax = std::max(E.yMax, Ei.yMax);
		E.zMin = std::min(E.zMin, Ei.zMin);
		E.zMax = std::max(E.zMax, Ei.zMax);
	}

	// Store the computed AABB as BVH node bounding box
	x1 = E.xMin;
	y1 = E.yMin;
	z1 = E.zMin;
	x2 = E.xMax;
	y2 = E.yMax;
	z2 = E.zMax;
}

// Set collider's World Matrix
inline void Collider::setWorldMatrix(glm::mat4 M) {
	Wm = M;
	// If it is a BVH, propagate the matrix to all the children
	for(auto* child : children) {
		child->setWorldMatrix(M);
	}
}

// Check collisions between a POINT collider and an AABB collider
// Note: a point collides with an AABB if, after being transformed in world space,
//		 it is located inside the transformed AABB's extents.
inline bool Collider::collisionPointAABB(Collider &P1l, Collider &P2l) {
	// P1l: POINT, P2L: AABB

	// Transform the point from local space to world space
	glm::vec3 P1 = glm::vec3(P1l.Wm * glm::vec4(P1l.x1, P1l.y1, P1l.z1, 1.0f));

	// Compute world space AABB extents
	EightPoints P2; 
	AABBextents E2;
	transformAABB(P2, P2l);
	getExtentsAABB(P2, E2); // Compute min/max from transformed positions

	// Collision if the point is within X, Y, Z axes
	return ((P1.x >= E2.xMin) && (P1.x <= E2.xMax) &&
			(P1.y >= E2.yMin) && (P1.y <= E2.yMax) &&
			(P1.z >= E2.zMin) && (P1.z <= E2.zMax));
}

// Check collisions between a POINT collider and an OOBB collider
// Note: To check if a point collides with an OOBB, we first transform the point in world space,
//		 then we bring the point into OOBB's local space, and finally we check if the point is located
//		 inside the OOBB.
inline bool Collider::collisionPointOOBB(Collider &P1l, Collider &P2l)
{
	// P1l: POINT, P2l: OOBB

	// Point in World Space
	glm::vec3 Pw = glm::vec3(P1l.Wm * glm::vec4(P1l.x1, P1l.y1, P1l.z1, 1.0f));

	// Transform the point into OOBB's Local Space
	glm::mat4 inv = glm::inverse(P2l.Wm);
	glm::vec3 P = glm::vec3(inv * glm::vec4(Pw, 1.0f));

	// An OOBB in local space is an AABB, thus we check the collision like in the AABB case
	return (P.x >= P2l.x1 && P.x <= P2l.x2 &&
			P.y >= P2l.y1 && P.y <= P2l.y2 &&
			P.z >= P2l.z1 && P.z <= P2l.z2);
}

// Check collisions between two AABB colliders
// Note: here we just check the overlapping along each axis
inline bool Collider::collisionAABBAABB(Collider &P1l, Collider &P2l) {
	// P1l: AABB, P2l: AABB

	EightPoints P1, P2; 
	AABBextents E1, E2;

	// Transform both boxes in world space
	transformAABB(P1, P1l);
	transformAABB(P2, P2l);

	// Compute the extents (min/max) of both boxes in world space
	getExtentsAABB(P1, E1);
	getExtentsAABB(P2, E2);

	// Collides if there is an overlapping in each axis
	return ((E1.xMax >= E2.xMin) && (E1.xMin <= E2.xMax) &&
			(E1.yMax >= E2.yMin) && (E1.yMin <= E2.yMax) &&
			(E1.zMax >= E2.zMin) && (E1.zMin <= E2.zMax));
}

// This function projects the 8 vertices of a box on the axes and returns min/max
// Note: this function is used for SAT (Separating Axis Theorem)
inline void Collider::getMinMaxAlongAxes(EightPoints P, glm::vec3 Axis, float &Min, float &Max) {
	float d = glm::dot(Axis, P.P[0]);
	Min = Max = d;
	
	for(int i = 1; i < 8; i++) {
		d = glm::dot(Axis, P.P[i]);
		if(d < Min) {Min = d;}
		if(d > Max) {Max = d;}
	}
}

// This functions checks for overlap between two boxes projected on an axis
// Note: this function is used for SAT (Separating Axis Theorem)
inline bool Collider::checkIntersectAxis(EightPoints P1, EightPoints P2, glm::vec3 Axis) {
	float P1min, P1max, P2min, P2max;
	
	getMinMaxAlongAxes(P1, Axis, P1min, P1max);
	getMinMaxAlongAxes(P2, Axis, P2min, P2max);

	// If ranges do not intersect, then it is a separating axis, and so there is no collision
	return ((P1max >= P2min) && (P1min <= P2max));
}

// Check collisions between an OOBB collider and an AABB collider
// Note: to check for collisions between OOBB and AABB colliders, we use the SAT (Separating Axis Theorem),
//		 using 15 test axes (3 OOBB's axes, 3 AABB's axes, 9 cross product crossed axes)
inline bool Collider::collisionOOBBAABB(Collider &P1l, Collider &P2l) {
	// P1l: OOBB, P2l: AABB

	EightPoints P1, P2;

	transformAABB(P1, P1l);
	transformAABB(P2, P2l);

	// Get OOBB's axes
	glm::mat4 m = P1l.Wm;
	glm::vec3 P1ax = glm::vec3(m * glm::vec4(1,0,0,0));
	glm::vec3 P1ay = glm::vec3(m * glm::vec4(0,1,0,0));
	glm::vec3 P1az = glm::vec3(m * glm::vec4(0,0,1,0));

	// 15 axes of the SAT (Separating Axis Theorem)
	const glm::vec3 Axes[15] = {
		// OOBB's axes
		P1ax, P1ay, P1az,
		// AABB's axes
		{1,0,0},{0,1,0},{0,0,1},
		// Cross products (OOBB x AABB) axes
		glm::cross(P1ax,{1,0,0}), glm::cross(P1ax,{0,1,0}), glm::cross(P1ax,{0,0,1}),
		glm::cross(P1ay,{1,0,0}), glm::cross(P1ay,{0,1,0}), glm::cross(P1ay,{0,0,1}),
		glm::cross(P1az,{1,0,0}), glm::cross(P1az,{0,1,0}), glm::cross(P1az,{0,0,1})
	};

	// SAT check: if one of the axis is a separating axis, then there is no collision
	for(int i=0;i<15;i++)
		if(glm::length(Axes[i])>0 && !checkIntersectAxis(P1,P2,glm::normalize(Axes[i])))
			return false;

	// If there is no separating axis, then there is a collision
	return true;
}

// Check collisions between two OOBB colliders
// Note: here we use a trick: we bring the first OOBB into its self local space.
//		 In this way, the first OOBB becomes an AABB, and we can reuse collisionOOBBAABB.
inline bool Collider::collisionOOBBOOBB(Collider &P1l, Collider &P2l)
{
	// P1l: OOBB, P2l: OOBB

	glm::mat4 inv = glm::inverse(P1l.Wm);

	// Transform the first OOBB into its self local space (it becomes an AABB)
	Collider AABB = P1l;
	AABB.setWorldMatrix(inv * P1l.Wm);

	// Transform the second OOBB into first OOBB's local space
	Collider OOBB = P2l;
	OOBB.setWorldMatrix(inv * P2l.Wm);

	// Check collisions as OOBB-AABB case
	return collisionOOBBAABB(OOBB, AABB);
}

// This function transforms the 8 AABB's vertices in world space
inline void Collider::transformAABB(EightPoints &Ps, Collider &C) {

	// Local vertices of the AABB
	const glm::vec3 Pl[8] = {
		{C.x1, C.y1, C.z1},
		{C.x1, C.y1, C.z2},
		{C.x1, C.y2, C.z1},
		{C.x1, C.y2, C.z2},
		{C.x2, C.y1, C.z1},
		{C.x2, C.y1, C.z2},
		{C.x2, C.y2, C.z1},
		{C.x2, C.y2, C.z2}
	};

	// Transform AABB's vertices using the world matrix
	for (int i = 0; i < 8; ++i) {
		Ps.P[i] = glm::vec3(C.Wm * glm::vec4(Pl[i], 1.0f));
	}
}

// This function computes min/max of world space AABB's points
inline void Collider::getExtentsAABB(EightPoints &Ps, AABBextents &E) {
	E.xMin = E.xMax = Ps.P[0].x;
	E.yMin = E.yMax = Ps.P[0].y;
	E.zMin = E.zMax = Ps.P[0].z;

	for (int i = 1; i < 8; ++i) {
		if (Ps.P[i].x < E.xMin) E.xMin = Ps.P[i].x;
		if (Ps.P[i].x > E.xMax) E.xMax = Ps.P[i].x;
		if (Ps.P[i].y < E.yMin) E.yMin = Ps.P[i].y;
		if (Ps.P[i].y > E.yMax) E.yMax = Ps.P[i].y;
		if (Ps.P[i].z < E.zMin) E.zMin = Ps.P[i].z;
		if (Ps.P[i].z > E.zMax) E.zMax = Ps.P[i].z;
	}
}

// This function returns the radius of the sphere, scaled accordingly with the world matrix
inline float Collider::getTransformedRadius(Collider &C) {

	// Scale of each world matrix's axis
	float sx = glm::length(glm::vec3(C.Wm[0]));
	float sy = glm::length(glm::vec3(C.Wm[1]));
	float sz = glm::length(glm::vec3(C.Wm[2]));

	// Use the largest component
	float s = (sx > sy ? sx : sy);
	s = (s > sz ? s : sz);
	
	return C.r * s;
}

// Check collisions between a SPHERE collider and an AABB collider
inline bool Collider::collisionSphereAABB(Collider &P1l, Collider &P2l) {
	// P1l: SPHERE, P2l: AABB

	// Radius of the sphere in world space (considering potential scaling of the sphere)
	float r = getTransformedRadius(P1l);

	// Center of the sphere in world space
	glm::vec3 P1 = glm::vec3(P1l.Wm * glm::vec4(P1l.x1, P1l.y1, P1l.z1, 1.0f));

	// Compute world space AABB extents
	EightPoints P2; 
	AABBextents E2;
	transformAABB(P2, P2l);
	getExtentsAABB(P2, E2);

	// Find the AABB's closest point to the sphere center
	glm::vec3 cP = getClosestPointOnAABB(E2, P1);

	// Collision if the closest point is within the radius
	return (glm::length(P1 - cP) <= r);
}

// Check collisions between a SPHERE collider and an OOBB collider
// Note: robust method:	1) Express the sphere center in OOBB coordinate system
//						2) Clamp each axis distance to the OOBB extent
//						3) Reconstruct closest point on the OOBB
//						4) collision if (distance)^2 <= (radius)^2
inline bool Collider::collisionSphereOOBB(Collider &P1l, Collider &P2l) {
    // P1l: SPHERE, P2l: OOBB

	// Center of the sphere in world space
    glm::vec3 centerSphere = glm::vec3(
        P1l.Wm * glm::vec4(P1l.x1, P1l.y1, P1l.z1, 1.0f)
    );

	// Radius of the sphere in world space (considering potential scaling of the sphere)
    float r = getTransformedRadius(P1l);

	// Center of the OOBB in local space
    glm::vec3 centerLocal(
        0.5f * (P2l.x1 + P2l.x2),
        0.5f * (P2l.y1 + P2l.y2),
        0.5f * (P2l.z1 + P2l.z2)
    );

	// Center of the OOBB in world space
    glm::vec3 centerBox = glm::vec3(
        P2l.Wm * glm::vec4(centerLocal, 1.0f)
    );

	// OOBB's axes in world space
    glm::vec3 axisX = glm::vec3(P2l.Wm * glm::vec4(1,0,0,0));
    glm::vec3 axisY = glm::vec3(P2l.Wm * glm::vec4(0,1,0,0));
    glm::vec3 axisZ = glm::vec3(P2l.Wm * glm::vec4(0,0,1,0));

	// Normalization
    float lenX = glm::length(axisX);
    float lenY = glm::length(axisY);
    float lenZ = glm::length(axisZ);
	// Security check for potential 0 scale
    if (lenX > 0.0f) axisX /= lenX;
    if (lenY > 0.0f) axisY /= lenY;
    if (lenZ > 0.0f) axisZ /= lenZ;

	// Half-extents
    float hx = 0.5f * (P2l.x2 - P2l.x1);
    float hy = 0.5f * (P2l.y2 - P2l.y1);
    float hz = 0.5f * (P2l.z2 - P2l.z1);

	// Scale extents using actual OOBB scaling
    float ex = hx * lenX;
    float ey = hy * lenY;
    float ez = hz * lenZ;

	// Vector centerSphere -> centerBox
    glm::vec3 v = centerSphere - centerBox;

	// Projections over the 3 axes of the OOBB
    float dx = glm::dot(v, axisX);
    float dy = glm::dot(v, axisY);
    float dz = glm::dot(v, axisZ);

	// Clamp inside extents
    float clampedX = glm::clamp(dx, -ex, ex);
    float clampedY = glm::clamp(dy, -ey, ey);
    float clampedZ = glm::clamp(dz, -ez, ez);

	// Closest point of the OOBB
    glm::vec3 closest =
        centerBox +
        clampedX * axisX +
        clampedY * axisY +
        clampedZ * axisZ;

	// Distance from the sphere
    glm::vec3 diff = centerSphere - closest;
    float dist2 = glm::dot(diff, diff);

	// Collision if closest point within radius
    return dist2 <= r * r;
}

// This function computes the closest point on an AABB to a given point P
inline glm::vec3 Collider::getClosestPointOnAABB(AABBextents &E, glm::vec3 P) {

	return glm::vec3(
		glm::clamp(P.x, E.xMin, E.xMax),
		glm::clamp(P.y, E.yMin, E.yMax),
		glm::clamp(P.z, E.zMin, E.zMax)
	);
}

// Check collisions between a POINT collider and a SPHERE collider
inline bool Collider::collisionPointSphere(Collider &P1l, Collider &P2l) {
	// P1l: POINT, P2l: SPHERE

	float r = getTransformedRadius(P2l);
	glm::vec3 P1 = glm::vec3(P1l.Wm * glm::vec4(P1l.x1, P1l.y1, P1l.z1, 1.0f));
	glm::vec3 P2 = glm::vec3(P2l.Wm * glm::vec4(P2l.x1, P2l.y1, P2l.z1, 1.0f));

	// Collision if the distance point-sphereCenter <= sphereRadius
	return (glm::length(P2 - P1) <= r);
}

// Check collisions between two SPHERE colliders
inline bool Collider::collisionSphereSphere(Collider &P1l, Collider &P2l) {
	// P1l: SPHERE, P2l: SPHERE

	float r1 = getTransformedRadius(P1l);
	float r2 = getTransformedRadius(P2l);
	glm::vec3 P1 = glm::vec3(P1l.Wm * glm::vec4(P1l.x1, P1l.y1, P1l.z1, 1.0f));
	glm::vec3 P2 = glm::vec3(P2l.Wm * glm::vec4(P2l.x1, P2l.y1, P2l.z1, 1.0f));

	// Collision if distance between centers <= sum of radii
	return (glm::length(P2 - P1) <= r1 + r2);
}

// Check collisions between two POINT colliders
inline bool Collider::collisionPointPoint(Collider &P1l, Collider &P2l) {
	// P1l: POINT, P2l: POINT

	glm::vec3 P1 = glm::vec3(P1l.Wm * glm::vec4(P1l.x1, P1l.y1, P1l.z1, 1.0f));
	glm::vec3 P2 = glm::vec3(P2l.Wm * glm::vec4(P2l.x1, P2l.y1, P2l.z1, 1.0f));

	// Collision if world space coordinates exactly match
	return ((P1.x == P2.x) && (P1.y == P2.y) && (P1.z == P2.z));
}

// Check collisions between a BVH collider and another collider
// Note: the algorithm is:	1) Compute this node's AABB (union of children)
//							2) If the other collider does not intersect this AABB, then there is no collision
//							3) If it's a leaf, check direct collider
//							4) Otherwise recursively check children
inline bool Collider::collisionBVHCollider(Collider &P1l, Collider &P2l) {
	// P1l: BVH, P2l: a generic collider

	// Compute the bounding box (AABB) of this BVH node
	AABBextents E = P1l.getExtents();

	Collider box;
	box.initAABB(E.xMin, E.yMin, E.zMin, E.xMax, E.yMax, E.zMax);
	box.setWorldMatrix(glm::mat4(1));

	// If there is no collision with the AABB, then there is no collision with the children
	if(!box.collidesWith(P2l)) return false;

	// If it's a leaf, check direct collider
	if(P1l.children.empty()) return P1l.collidesWith(P2l);

	// Recursively check children
	for(auto child : P1l.children)
	{
		if(collisionBVHCollider(*child, P2l)) return true;
	}

	return false;
}

// This function acts as a dispatcher selecting the correct collision checking algorithm based on both collider types
inline bool Collider::collidesWith(Collider &dest) {
	switch(type) {
	  case CLD_POINT:
		switch(dest.type) {
		  case CLD_POINT:
		    return collisionPointPoint(*this, dest);
		  case CLD_AABB:
		    return collisionPointAABB(*this, dest);
		  case CLD_OOBB:
			return collisionPointOOBB(*this, dest);
		  case CLD_SPHERE:
		    return collisionPointSphere(*this, dest);
		  case CLD_BVH:
		    return collisionBVHCollider(dest, *this);
		}
	  case CLD_AABB:
		switch(dest.type) {
		  case CLD_POINT:
		    return collisionPointAABB(dest, *this);
		  case CLD_AABB:
		    return collisionAABBAABB(*this, dest);
		  case CLD_OOBB:
			return collisionOOBBAABB(dest, *this);
		  case CLD_SPHERE:
		    return collisionSphereAABB(dest, *this);
		  case CLD_BVH:
		    return collisionBVHCollider(dest, *this);
		}
	  case CLD_OOBB:
		switch(dest.type) {
		  case CLD_POINT:
			return collisionPointOOBB(dest, *this);
		  case CLD_AABB:
			return collisionOOBBAABB(*this, dest);
		  case CLD_OOBB:
			return collisionOOBBOOBB(*this, dest);
		  case CLD_SPHERE:
			return collisionSphereOOBB(dest, *this);
		  case CLD_BVH:
		    return collisionBVHCollider(dest, *this);
		}
	  case CLD_SPHERE:
		switch(dest.type) {
		  case CLD_POINT:
		    return collisionPointSphere(dest, *this);
		  case CLD_AABB:
		    return collisionSphereAABB(*this, dest);
		  case CLD_OOBB:
			return collisionSphereOOBB(*this, dest);
		  case CLD_SPHERE:
		    return collisionSphereSphere(*this, dest);
		  case CLD_BVH:
		    return collisionBVHCollider(dest, *this);
		}
	  case CLD_BVH:
		return collisionBVHCollider(*this, dest);
	}
	
	std::cout << "Collision not supported between types\n";
	return false;
}

// Compute the world space AABB extents of any collider
// Note: it is used for BVH
inline AABBextents Collider::getExtents() {
	AABBextents E, E2;
	glm::vec3 P;
	float r;
	EightPoints P8; 
		
	switch(type) {
	  case CLD_POINT:
		// Extents collapse to the point position
	    P = glm::vec3(Wm * glm::vec4(x1, y1, z1, 1.0f));
		E.xMin = E.xMax = P.x;
		E.yMin = E.yMax = P.y;
		E.zMin = E.zMax = P.z;
	    break;

	  case CLD_AABB:
	  case CLD_OOBB:
		// Transform the 8 corners, then compute min/max
		transformAABB(P8, *this);
		getExtentsAABB(P8, E);
		break;

	  case CLD_SPHERE:
		// Sphere produces an AABB centered on its world center
	    P = glm::vec3(Wm * glm::vec4(x1, y1, z1, 1.0f));
		r = getTransformedRadius(*this);
		E.xMin = P.x - r;
		E.yMin = P.y - r;
		E.zMin = P.z - r;
		E.xMax = P.x + r;
		E.yMax = P.y + r;
		E.zMax = P.z + r;
	    break;

	  case CLD_BVH:
		// BVH: union of children's extents
		E = children[0]->getExtents();
		for(int i = 1; i < children.size(); i++) {
			E2 = children[i]->getExtents();
			E.xMin = E2.xMin < E.xMin ? E2.xMin : E.xMin;
			E.xMax = E2.xMax > E.xMax ? E2.xMax : E.xMax;
			E.yMin = E2.yMin < E.yMin ? E2.yMin : E.yMin;
			E.yMax = E2.yMax > E.yMax ? E2.yMax : E.yMax;
			E.zMin = E2.zMin < E.zMin ? E2.zMin : E.zMin;
			E.zMax = E2.zMax > E.zMax ? E2.zMax : E.zMax;
		}
	    break;

	  default:
		std::cout << "getExtent not supported by this collider type\n";
	    break;
	}

	return E;
}




// COLLIDERS VISUALIZATION
class ColliderShow;

#define MAX_COLLIDERS 20
#define SPHERE_RESOLUTION 64

// Uniform buffer used by the collider rendering pipeline
struct ColliderShowUniformBufferObject {
	alignas(16) glm::mat4 vpMat;
	alignas(16) glm::vec4 strokeColors[MAX_COLLIDERS];
};

// Vertex format used to draw collider wireframes
struct ColliderShowVertex {
	glm::vec3 pos;
};

// Helper structure used to pass both the ColliderShow instance
// and the generated Model to the command buffer callback.
struct ColliderShowAndModel {
	ColliderShow *cls;
	Model *M;
};

// Push constants sent to the vertex shader
// Note: push constants are extremely fast and ideal for per-draw-call parameters.
struct ColliderShowPushConstant {
	alignas(4) int colliderIndex;	// Index in the array of colliders being drawn
	alignas(4) int colliderType;	// Type of collider (AABB, OOBB, SPHERE, ...)
};

// Internal representation of a collider ready for rendering.
struct ShownCollider {
	Collider *c;		// Pointer to the original Collider object
	int start;			// Start index in the shared index buffer
	int len;			// Number of indices to render
	glm::vec4 Stroke;	// Stroke color
};

// This class handles GPU visualization of all colliders.
// It builds wireframe meshes for AABB, OOBB, SPHERE, POINT and BVH, uploads them to Vulkan buffers,
// and renders them as line lists.
class ColliderShow {
	friend Collider;

	int submitOrder;

	// List of colliders currently rendered
	std::vector<ShownCollider> clds;

	// Vulkan objects
	DescriptorSetLayout DSL;
	VertexDescriptor VD;	
	
	BaseProject *BP;

	RenderPass RP;
	Pipeline P;
	Model *M = nullptr;
	DescriptorSet DS;

	// When colliders move or are added/removed, we must regenerate the mesh and recreate the command buffer.
	bool commandBufferMustUpdate = false;

	public:

	// This function initializes ColliderShow
	void init(BaseProject *_BP, int MaxColliders, int so=11000) {
		BP = _BP;
		submitOrder = so;

		// Vertex Descriptor initialization
		VD.init(BP, {
				  {0, sizeof(ColliderShowVertex), VK_VERTEX_INPUT_RATE_VERTEX}
				}, {
				  {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ColliderShowVertex, pos),
				         sizeof(glm::vec3), POSITION}
				});

		// Descriptor Set Layout initialization
		DSL.init(BP,
				{{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT,
				  sizeof(ColliderShowUniformBufferObject), 1}});

		// Pipeline initialization
		P.init(BP, &VD, "shaders/framework/ColliderShow.vert.spv", "shaders/framework/ColliderShow.frag.spv", {&DSL},
	{{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ColliderShowPushConstant)}});
		P.setCompareOp(VK_COMPARE_OP_ALWAYS);			// We disable depth testing so colliders are always visible over objects.
		P.setCullMode(VK_CULL_MODE_NONE);				// No culling (colliders are lines, not solid triangles).
		P.setTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);	// Draw as line segments.

		// Render Pass initialization
		RP.init(BP, -1, -1, -1,
					RenderPass::getStandardAttchmentsProperties(AT_SURFACE_NOAA_DEPTH, BP));
		RP.properties[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		RP.properties[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		RP.properties[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		RP.properties[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


		BP->DPSZs.uniformBlocksInPool += MaxColliders;	// Overprovision pools
		BP->DPSZs.setsInPool += MaxColliders;
	}

	// This function updates the shown colliders information to render them correctly
	void updateTransforms(uint32_t currentImage, const glm::mat4& vp) {
		ColliderShowUniformBufferObject ubo{};
		ubo.vpMat = vp;
		for (int i = 0; i < clds.size(); ++i) {
			ubo.strokeColors[i]= clds[i].Stroke;
		}
		DS.map(currentImage, &ubo, 0);
	}

	// This function pre-computes the number of vertices (sv) and indices (si) required
	// for the collider’s bounding geometry.
	// Note: this is necessary because a single shared Model is used to store the
	//		 geometry of all colliders together.
	void SizeBoundingGeometry(Collider *c, int &sv, int &si) {
		switch(c->type) {
			case CLD_AABB:
				// 8 corners, 12 edges -> 24 indices
				sv += 8;
				si += 24;
				break;
			case CLD_OOBB:
				// 8 corners, 12 edges -> 24 indices
				sv += 8;
				si += 24;
				break;
			case CLD_SPHERE:
				// Sphere drawn as 3 circles (XY, XZ, YZ)
				// Each circle: SPHERE_RESOLUTION segments -> 2 vertices per segment
				sv += SPHERE_RESOLUTION * 6;
				si += SPHERE_RESOLUTION * 12;
				break;
			case CLD_POINT:
				// Point drawn as 3 axis-aligned crosshair lines
				sv += 6;  // 3 axes * 2 vertices
				si += 6;  // Every line = 2 indices => 3 lines -> 6 indices
				break;
			case CLD_BVH:
				// Draw the BVH node AABB
				sv += 8;
				si += 24;

				// Recursively draw all the children
				for (auto child : c->children)
					SizeBoundingGeometry(child, sv, si);
				break;
		}
	}

	// This function generates the renderable wireframe of an AABB or of an OOBB
	// Note: in this and in the following methods the parameters are:
	//			- c: the collider being rendered
	//			- V_vertex: the pointer to the next free vertex slot in the vertex buffer
	//			- ib: the current write index in the index buffer
	// Note: for OOBB, we use the collider's world matrix to transform its local AABB corner coordinates.
	//		 For AABB, we recompute the global extents and rebuild an aligned AABB.
	void MakeBox(Collider *c, ColliderShowVertex *&V_vertex, int &ib, bool isAxisAligned) {

		// Compute the 8 world space vertices of the box (AABB or OOBB)
		EightPoints P8;
		c->transformAABB(P8, *c);

		// If we want an AABB, recompute the min/max extents
		if (isAxisAligned)
		{
			// Compute the AABB extents
			AABBextents E;
			c->getExtentsAABB(P8, E);

			// Overwrite P8 with axis-aligned corners
			P8.P[0] = {E.xMin, E.yMin, E.zMin};
			P8.P[1] = {E.xMin, E.yMin, E.zMax};
			P8.P[2] = {E.xMin, E.yMax, E.zMin};
			P8.P[3] = {E.xMin, E.yMax, E.zMax};
			P8.P[4] = {E.xMax, E.yMin, E.zMin};
			P8.P[5] = {E.xMax, E.yMin, E.zMax};
			P8.P[6] = {E.xMax, E.yMax, E.zMin};
			P8.P[7] = {E.xMax, E.yMax, E.zMax};
		}

		// Write the 8 vertices into the vertex buffer
		ColliderShowVertex* baseV = V_vertex;
		for (int i = 0; i < 8; ++i)
			(V_vertex++)->pos = P8.P[i];

		// Retrieve the base vertex index inside the model buffer
		int v0 = int((uint8_t*)baseV - (uint8_t*)&M->vertices[0]) / VD.Bindings[0].stride;

		// Helper lambda that writes two indices (one line)
		auto &I = M->indices;
		auto addEdge = [&](int a, int b){ I[ib++] = a; I[ib++] = b; };

		// Add the 12 edges (line list): 24 indices
		// Bottom Edges
		addEdge(v0+0, v0+1);
		addEdge(v0+0, v0+2);
		addEdge(v0+3, v0+1);
		addEdge(v0+3, v0+2);

		// Top Edges
		addEdge(v0+4, v0+5);
		addEdge(v0+4, v0+6);
		addEdge(v0+7, v0+5);
		addEdge(v0+7, v0+6);

		// Vertical Edges
		addEdge(v0+0, v0+4);
		addEdge(v0+1, v0+5);
		addEdge(v0+2, v0+6);
		addEdge(v0+3, v0+7);
	}

	// This function generates a sphere wireframe using 3 circles (XY, XZ, YZ planes)
	// Note: each circle has SPHERE_RESOLUTION segments (the higher this value, the smoother the circle is).
	//		 For each segment, we emit 2 vertices defining a line.
	void MakeSphere(Collider *c, ColliderShowVertex *&V_vertex, int &ib) {

		// Compute sphere center in world space
		glm::vec3 center = glm::vec3(c->Wm * glm::vec4(c->x1, c->y1, c->z1, 1.0f));

		// Compute the radius after world-scale transformation
		float r = c->getTransformedRadius(*c);

		// Base index of this sphere in the vertex buffer
		int baseIndex = (uint8_t*)V_vertex - (uint8_t*)&M->vertices[0];
		baseIndex /= VD.Bindings[0].stride;

		// Helper lambda to draw one circle defined by two perpendicular axes
		auto addCircle = [&](glm::vec3 ax1, glm::vec3 ax2){
			for(int i=0;i<SPHERE_RESOLUTION;i++){
				float a0 = float(i) / SPHERE_RESOLUTION * 6.2831853f;
				float a1 = float(i+1) / SPHERE_RESOLUTION * 6.2831853f;

				// Parametric form: center + r*(ax1*cosθ + ax2*sinθ)
				glm::vec3 p0 = center + r*(ax1*cos(a0) + ax2*sin(a0));
				glm::vec3 p1 = center + r*(ax1*cos(a1) + ax2*sin(a1));

				(V_vertex++)->pos = p0;
				(V_vertex++)->pos = p1;

				// Add corresponding indices
				M->indices[ib++] = baseIndex + (i*2);
				M->indices[ib++] = baseIndex + (i*2+1);
			}

			// Move base index for the next circle
			baseIndex += SPHERE_RESOLUTION * 2;
		};

		// Draw the three circles
		addCircle({1,0,0},{0,1,0}); // XY
		addCircle({1,0,0},{0,0,1}); // XZ
		addCircle({0,1,0},{0,0,1}); // YZ
	}

	// This function visualizes a point collider as a small crosshair aligned to world axes.
	// Note: three short line segments are emitted (one for each axis)
	void MakePoint(Collider *c, ColliderShowVertex *&V_vertex, int &ib) {

		glm::vec3 P = glm::vec3(c->Wm * glm::vec4(c->x1, c->y1, c->z1, 1.0f));

		const float L = 0.1f; // Half-length of axis crosshair

		// Endpoints of the 3 lines (X, Y, Z)
		glm::vec3 pts[] = {
			P + glm::vec3( L,0,0), P - glm::vec3(L,0,0), // X axis
			P + glm::vec3(0, L,0), P - glm::vec3(0,L,0), // Y axis
			P + glm::vec3(0,0, L), P - glm::vec3(0,0,L)  // Z axis
		};

		int base = ((uint8_t*)V_vertex - (uint8_t*)&M->vertices[0]) / VD.Bindings[0].stride;

		// Write the 6 vertices
		for(int i=0;i<6;i++)
		{
			(V_vertex++)->pos = pts[i];
		}

		// Add the 3 line segments (6 indices)
		M->indices[ib++] = base+0; M->indices[ib++] = base+1;
		M->indices[ib++] = base+2; M->indices[ib++] = base+3;
		M->indices[ib++] = base+4; M->indices[ib++] = base+5;
	}

	// This function dispatches to the correct geometry-construction function depending on the collider type
	// Note: BVH nodes are rendered as AABBs and processed recursively.
	void MakeBoundingGeometry(Collider *c, ColliderShowVertex *&V_vertex, int &ib) {
		switch(c->type) {
			case CLD_AABB:
				MakeBox(c, V_vertex, ib, true);	// Axis-aligned bounding box
				break;
			case CLD_OOBB:
				MakeBox(c, V_vertex, ib, false);	// Oriented bounding box
				break;
			case CLD_SPHERE:
				MakeSphere(c, V_vertex, ib);
				break;
			case CLD_POINT:
				MakePoint(c,V_vertex,ib);
				break;
			case CLD_BVH:
				// Draw the bounding box of the BVH node (AABB)
				MakeBox(c, V_vertex, ib, true);

				// Recursively draw the children
				for (auto child : c->children)
					MakeBoundingGeometry(child, V_vertex, ib);
				break;
			default:
				MakeBox(c, V_vertex, ib, false);
				break;
		}
	}

	// This function builds the GPU mesh used to draw all collider wireframes
	// Note: the process is the following:
	//			1) Count how many vertices/indices are needed for all colliders
	//			2) Allocate the required vertex/index buffers
	//			3) For each collider:
	//				- generate its geometry (wireframe)
	//				- record where its index range starts and how long it is
	//			4) Upload the mesh to the GPU
	// NOTE: each collider writes sequentially into the shared mesh buffer.
	//		 The field cld.start and cld.len tell Vulkan which part of the buffer corresponds
	//		 to each collider.
	void createColliderMesh() {

		// Create an empty model to store the generated mesh
		M = new Model();
		
		int mainStride = VD.Bindings[0].stride;	// Size of one ColliderShowVertex
		int ib = 0;  // Current index buffer write position
		int sv = 0;  // Total vertex count
		int si = 0;  // Total index count

		// 1) Count how many vertices/indices are needed for all colliders
		for(auto &cld : clds) {
			SizeBoundingGeometry(cld.c, sv, si);
		}

		// 2) Allocate buffers with exact required size
		M->indices.resize(si);
		M->vertices.resize(sv * mainStride);

		// Pointer to first vertex in the buffer (will be incremented)
		ColliderShowVertex *V_vertex = (ColliderShowVertex *)(&M->vertices[0]);

		// 3) Generate the geometry for each collider
		for(auto &cld : clds) {
			cld.start = ib;	// mark the first index written for this collider

			// Generate wireframe geometry (box, sphere, point, BVH recursive…)
			MakeBoundingGeometry(cld.c, V_vertex, ib);
			
			cld.len = ib - cld.start; // number of indices written
		}

		// 4) Upload geometry to GPU buffers
		M->initMesh(BP, &VD, true);
	}

	void pipelinesAndDescriptorSetsInit() {
		RP.create();
		P.create(&RP);
		DS.init(BP, &DSL, {});
	}

	void pipelinesAndDescriptorSetsCleanup() {
		P.cleanup();
		RP.cleanup();
		DS.cleanup();
	}

	void localCleanup() {
		if(M != nullptr) {
			M->cleanup();
		}
		DSL.cleanup();
		
		P.destroy();
		RP.destroy();
	}

	static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *Params) {
		//std::cout << "Populating access (" << commandBuffer << ") for image: " << currentImage << "\n";
		ColliderShow *T = ((ColliderShowAndModel *)Params)->cls;
		T->populateCommandBuffer(commandBuffer, currentImage);
	}

    void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {
		//std::cout << "Populating for image: " << currentImage << "\n";

		RP.begin(commandBuffer, currentImage);

    	P.bind(commandBuffer);
		M->bind(commandBuffer);
		DS.bind(commandBuffer, P, 0, currentImage);

		// Draw each collider individually
		for (int i = 0; i < clds.size(); ++i) {
			ColliderShowPushConstant PKv{};
			PKv.colliderIndex = i;

			vkCmdPushConstants(
				commandBuffer,
				P.pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT,
				0,
				sizeof(PKv),
				&PKv
			);

			vkCmdDrawIndexed(commandBuffer,
							 static_cast<uint32_t>(clds[i].len),	// Index count
							 1,										// Instance count
							 static_cast<uint32_t>(clds[i].start),	// First index
							 0, 0);
		}

		RP.end(commandBuffer);			
	}

	static void freeCommandBuffer(void *Params) {
		Model *M = ((ColliderShowAndModel *)Params)->M;
		M->cleanup();
		
		free(Params);
	}	

	void updateCommandBuffer() {
		if (clds.empty()) return;

		if(commandBufferMustUpdate) {

			// Rebuild mesh geometry (CPU-side)
			createColliderMesh();

			// Submit a new command buffer generation task
			//std::cout << "Submitting command buffer\n";
			ColliderShowAndModel *tm = (ColliderShowAndModel *)malloc(sizeof(ColliderShowAndModel));
			tm->cls = this;
			tm->M = M;
			BP->submitCommandBuffer("collshow",
								submitOrder,
								ColliderShow::populateCommandBufferAccess,
								tm,
								ColliderShow::freeCommandBuffer);
			//std::cout << "Submitted\n";

			commandBufferMustUpdate = false;
		}
	}

	// This function adds a new collider to be shown
	// Note: stroke set to green by default
	void show(Collider *c) {
		clds.push_back({c, 0, 0, {0.0f, 1.0f, 0.0f, 1.0f}});
		commandBufferMustUpdate = true;
	}

	// This function updates render pass size
	// Note: it's called when the screen window changes size.
	void resizeScreen(int sW, int sH) {
		RP.width = sW;
		RP.height = sH;
		commandBufferMustUpdate = true;
	}

	// Set stroke's color of a collider, searching by pointer
	// Note: this function works always, even when dynamically adding/removing some
	//		 shown colliders.
	void setStroke(Collider* c, glm::vec4 color) {
		for (auto& sc : clds) {
			if (sc.c == c) {
				sc.Stroke = color;
				break;
			}
		}
	}

	// Set stroke's color of a collider, using collider's index in the list
	// Note: this function is more efficient than the other, but you must be sure
	//		 about shown colliders' indices.
	void setStrokeByIndex(int index, glm::vec4 color) {
		clds[index].Stroke = color;
	}

	// This function forces a refresh (rebuild mesh and re-record command buffer)
	void refresh()
	{
		commandBufferMustUpdate = true;
		updateCommandBuffer();
	}
};