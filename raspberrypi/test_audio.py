import pygame

freq = 44100
bitsize = - 16
channels = 1
buffer = 2048

pygame.mixer.init()
#while True:
music_file = "Dontgo.wav"
pygame.mixer.music.load(music_file)
pygame.mixer.music.play()
clock = pygame.time.Clock()
while pygame.mixer.music.get_busy():
    clock.tick(5)


pygame.mixer.quit()
