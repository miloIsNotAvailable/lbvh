#include <lens.hpp>

Lens::Lens( glm::vec3 point, glm::vec3 dir ) : point( glm::vec4(point, 0.f) ), dir( glm::vec4(dir, 0.f) ) {}

Camera::Camera( float sensorDist, float focalPlaneDist, float apertureSize, 
                glm::vec3& e, glm::vec3& c ) : 
focalPlaneDist( focalPlaneDist ), 
sensorDist( sensorDist ), 
apertureSize( apertureSize ) {

    eye = glm::vec4( e, 0.f );
    center = glm::vec4( c, 0.f );

    n = glm::normalize(center - eye);
    sensor = eye + n * sensorDist;
	focalPlane =  eye + n * focalPlaneDist;
    
    // printf( "sensor: %f, %f, %f\n", sensor.x, sensor.y, sensor.z );
    // printf( "%f, %f, %f\n", center.x, center.y, center.z );

    worldUp = glm::vec4( 0., 1., 0., 0. );
 
    if (glm::abs(glm::dot(n, worldUp)) > 0.999f) {
        worldUp = glm::vec4(1.0f, 0.0f, 0.0f, 0.f);
    }
    
    glm::vec3 right3 = glm::normalize(glm::cross( glm::vec3(worldUp), glm::vec3(n) ));
    glm:: vec3 up3 = glm::normalize(glm::cross(glm::vec3(n), right3));

    right = glm::vec4( right3, 0.f );
    up = glm::vec4( up3, 0.f );
}

Lens Camera::thinLensRay( glm::vec2& st )
{

    float r = hash() * apertureSize;
    float a = hash() * (2. * PI);
    glm::vec2 rndPointOnAperture = glm::vec2(r * cos(a), r * sin(a));
    
    glm::vec3 rndAperturePointWrld = eye + rndPointOnAperture.x * right + rndPointOnAperture.y * up;
    
    glm::vec3 pixelWrld = sensor + st.x * right + st.y * up;
    
    glm::vec3 primaryRay = glm::normalize( pixelWrld-glm::vec3(eye) );
    
    glm::vec3 F_c = focalPlane - eye;
    float dirDotNor = dot(primaryRay, glm::vec3(n));
    float focalPlaneParam = dot(F_c, glm::vec3(n)) / dirDotNor;
    glm::vec3 focalPlanePoint = eye + glm::vec4(primaryRay, 0.f) * focalPlaneParam;
    
    glm::vec3 rayDir = glm::normalize( focalPlanePoint - rndAperturePointWrld );
    return Lens(rndAperturePointWrld, rayDir);
}