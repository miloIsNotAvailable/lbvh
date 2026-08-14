#include <lens.hpp>

Lens::Lens( glm::vec3 point, glm::vec3 dir ) : point( point ), dir( dir ) {}

Camera::Camera( float sensorDist, float focalPlaneDist, float apertureSize, 
                glm::vec3& eye, glm::vec3& center ) : 
focalPlaneDist( focalPlaneDist ), 
sensorDist( sensorDist ), 
apertureSize( apertureSize ),
eye( eye ), center( center ) {

    n = glm::normalize(center - eye);
    sensor = eye + n * sensorDist;
	focalPlane =  eye + n * focalPlaneDist;
    
    // printf( "sensor: %f, %f, %f\n", sensor.x, sensor.y, sensor.z );
    // printf( "%f, %f, %f\n", center.x, center.y, center.z );

    worldUp = glm::vec3( 0., 1., 0. );
 
    if (glm::abs(glm::dot(n, worldUp)) > 0.999f) {
        worldUp = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    
    right = glm::normalize(cross( worldUp, n ));
    up = glm::normalize(cross(n, right));
}

Lens Camera::thinLensRay( glm::vec2& st )
{

    float r = hash() * apertureSize;
    float a = hash() * (2. * PI);
    glm::vec2 rndPointOnAperture = glm::vec2(r * cos(a), r * sin(a));
    
    glm::vec3 rndAperturePointWrld = eye + rndPointOnAperture.x * right + rndPointOnAperture.y * up;
    
    glm::vec3 pixelWrld = sensor + st.x * right + st.y * up;
    
    glm::vec3 primaryRay = glm::normalize( pixelWrld-eye );
    
    glm::vec3 F_c = focalPlane - eye;
    float dirDotNor = dot(primaryRay, n);
    float focalPlaneParam = dot(F_c, n) / dirDotNor;
    glm::vec3 focalPlanePoint = eye + primaryRay * focalPlaneParam;
    
    glm::vec3 rayDir = glm::normalize( focalPlanePoint - rndAperturePointWrld );
    return Lens(rndAperturePointWrld, rayDir);
}